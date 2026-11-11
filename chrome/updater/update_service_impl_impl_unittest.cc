// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service_impl_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/branded_constants.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/external_constants_builder.h"
#include "chrome/updater/external_constants_default.h"
#include "chrome/updater/external_constants_override.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/prefs_impl.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/remove_uninstalled_apps_task.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "components/crx_file/crx_verifier.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/crx_cache.h"
#include "components/update_client/network.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/ui/l10n_util.h"
#include "chrome/updater/win/ui/resources/updater_installer_strings.h"
#endif  // BUILDFLAG(IS_WIN)

namespace updater::internal {
namespace {

// Answers every POST with `response_body` and reports each request body to
// `on_post_request`. Downloads are not supported.
class FakeUpdateCheckFetcher : public update_client::NetworkFetcher {
 public:
  FakeUpdateCheckFetcher(
      std::string response_body,
      base::RepeatingCallback<void(const std::string&)> on_post_request)
      : response_body_(std::move(response_body)),
        on_post_request_(std::move(on_post_request)) {}
  ~FakeUpdateCheckFetcher() override = default;

  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback) override {
    on_post_request_.Run(post_data);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::string response_body,
               ResponseStartedCallback response_started_callback,
               PostRequestCompleteCallback post_request_complete_callback) {
              response_started_callback.Run(
                  /*response_code=*/200,
                  /*content_length=*/response_body.size());
              std::move(post_request_complete_callback)
                  .Run(std::move(response_body), /*net_error=*/0,
                       /*header_etag=*/"", /*header_x_cup_server_proof=*/"",
                       /*header_set_cookie=*/"",
                       /*xheader_retry_after_sec=*/-1);
            },
            response_body_, std::move(response_started_callback),
            std::move(post_request_complete_callback)));
  }

  base::OnceClosure DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadToFileCompleteCallback download_to_file_complete_callback)
      override {
    NOTREACHED() << "Unexpected download of " << url;
  }

 private:
  const std::string response_body_;
  base::RepeatingCallback<void(const std::string&)> on_post_request_;
};

// Creates `FakeUpdateCheckFetcher` instances that share `response_body` and
// `on_post_request`.
class FakeUpdateCheckFetcherFactory
    : public update_client::NetworkFetcherFactory {
 public:
  FakeUpdateCheckFetcherFactory(
      std::string response_body,
      base::RepeatingCallback<void(const std::string&)> on_post_request)
      : response_body_(std::move(response_body)),
        on_post_request_(std::move(on_post_request)) {}

  std::unique_ptr<update_client::NetworkFetcher> Create() const override {
    return std::make_unique<FakeUpdateCheckFetcher>(response_body_,
                                                    on_post_request_);
  }

 protected:
  ~FakeUpdateCheckFetcherFactory() override = default;

 private:
  const std::string response_body_;
  base::RepeatingCallback<void(const std::string&)> on_post_request_;
};

// A `Configurator` that uses `network_fetcher_factory`, a CRX cache at
// `crx_cache_path`, and an unmetered connection.
class UpdateAllTestConfigurator : public Configurator {
 public:
  UpdateAllTestConfigurator(scoped_refptr<UpdaterPrefs> prefs,
                            scoped_refptr<ExternalConstants> external_constants,
                            UpdaterScope scope,
                            scoped_refptr<update_client::NetworkFetcherFactory>
                                network_fetcher_factory,
                            const base::FilePath& crx_cache_path)
      : Configurator(std::move(prefs), std::move(external_constants), scope),
        network_fetcher_factory_(std::move(network_fetcher_factory)),
        crx_cache_(
            base::MakeRefCounted<update_client::CrxCache>(crx_cache_path)) {}

  scoped_refptr<update_client::NetworkFetcherFactory> GetNetworkFetcherFactory()
      override {
    return network_fetcher_factory_;
  }

  scoped_refptr<update_client::CrxCache> GetCrxCache() const override {
    return crx_cache_;
  }

  bool IsConnectionMetered() const override { return false; }

 protected:
  ~UpdateAllTestConfigurator() override = default;

 private:
  scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory_;
  scoped_refptr<update_client::CrxCache> crx_cache_;
};

}  // namespace

TEST(UpdateServiceImplTest, TestToResult) {
  EXPECT_EQ(ToResult(update_client::Error::NONE),
            UpdateService::Result::kSuccess);
  EXPECT_EQ(ToResult(update_client::Error::UPDATE_IN_PROGRESS),
            UpdateService::Result::kUpdateInProgress);
  EXPECT_EQ(ToResult(update_client::Error::UPDATE_CANCELED),
            UpdateService::Result::kUpdateCanceled);
  EXPECT_EQ(ToResult(update_client::Error::RETRY_LATER),
            UpdateService::Result::kRetryLater);
  EXPECT_EQ(ToResult(update_client::Error::SERVICE_ERROR),
            UpdateService::Result::kServiceFailed);
  EXPECT_EQ(ToResult(update_client::Error::UPDATE_CHECK_ERROR),
            UpdateService::Result::kUpdateCheckFailed);
  EXPECT_EQ(ToResult(update_client::Error::CRX_NOT_FOUND),
            UpdateService::Result::kAppNotFound);
  EXPECT_EQ(ToResult(update_client::Error::INVALID_ARGUMENT),
            UpdateService::Result::kInvalidArgument);
  EXPECT_EQ(ToResult(update_client::Error::BAD_CRX_DATA_CALLBACK),
            UpdateService::Result::kInvalidArgument);
}

TEST(UpdateServiceImplTest, TestGetComponentsInOrder) {
  base::test::TaskEnvironment environment_;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath plist_path = temp_dir.GetPath().Append(
      FILE_PATH_LITERAL("InstallerTest.LoadFromPath.plist"));
  base::WriteFile(plist_path, "<dict></dict>");

  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());
  auto metadata = base::MakeRefCounted<PersistedData>(
      GetUpdaterScopeForTesting(), pref.get(), nullptr);
  metadata->SetProductVersion("id1", base::Version("1.2.3.4"));
  metadata->SetProductVersionKey("id1", "pv_key");
  metadata->SetAP("id1", "ap");
  metadata->SetAPPath("id1", plist_path);
  metadata->SetProductVersionPath("id1", plist_path);
  metadata->SetBrandPath("id1", plist_path);
  metadata->SetAPKey("id1", "brand_key");
  metadata->SetBrandCode("id1", "BRND");

  std::vector<std::optional<update_client::CrxComponent>> crxs;
  base::RunLoop loop;
  internal::GetComponents(
      base::MakeRefCounted<PolicyService>(CreateExternalConstants(),
                                          /*persisted_data=*/nullptr),
      crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF, std::nullopt,
      metadata, {}, {}, {}, UpdateService::Priority::kForeground, false,
      UpdateService::PolicySameVersionUpdate::kNotAllowed,
      {"id1", "id2", "id3", "id4"},
      base::BindLambdaForTesting(
          [&](const std::vector<std::optional<update_client::CrxComponent>>&
                  out) {
            crxs = out;
            loop.Quit();
          }));
  loop.Run();

  ASSERT_EQ(crxs.size(), 4u);
  EXPECT_EQ(crxs[0]->app_id, "id1");
  EXPECT_EQ(crxs[1]->app_id, "id2");
  EXPECT_EQ(crxs[2]->app_id, "id3");
  EXPECT_EQ(crxs[3]->app_id, "id4");
}

TEST(UpdateServiceImplTest, UpdateAllShufflesApps) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::test::TaskEnvironment task_environment;

  // `UpdateAll` requires the updater itself to be registered.
  std::vector<std::string> registered_apps = {kUpdaterAppId};
  for (int i = 1; i <= 9; ++i) {
    registered_apps.push_back(absl::StrFormat("app%d", i));
  }

  base::ListValue response_apps;
  for (const std::string& app_id : registered_apps) {
    response_apps.Append(
        base::DictValue()
            .Set("appid", app_id)
            .Set("status", "ok")
            .Set("updatecheck", base::DictValue().Set("status", "noupdate")));
  }
  std::string response_json;
  ASSERT_TRUE(base::JSONWriter::Write(
      base::DictValue().Set("response",
                            base::DictValue()
                                .Set("protocol", "4.0")
                                .Set("apps", std::move(response_apps))),
      &response_json));
  const std::string response_body = base::StrCat({")]}'\n", response_json});

  std::vector<std::string> captured_requests;
  auto pref = std::make_unique<TestingPrefServiceSimple>();
  update_client::RegisterPrefs(pref->registry());
  RegisterPersistedDataPrefs(pref->registry());

  base::DictValue overrides;
  overrides.Set(kDevOverrideKeyUseCUP, false);
  overrides.Set(kDevOverrideKeyUrl, "https://localhost/update");

  auto configurator = base::MakeRefCounted<UpdateAllTestConfigurator>(
      base::MakeRefCounted<UpdaterPrefsImpl>(temp_dir.GetPath(),
                                             /*lock=*/nullptr, std::move(pref)),
      base::MakeRefCounted<ExternalConstantsOverrider>(
          std::move(overrides), CreateDefaultExternalConstants()),
      GetUpdaterScopeForTesting(),
      base::MakeRefCounted<FakeUpdateCheckFetcherFactory>(
          response_body,
          base::BindLambdaForTesting([&](const std::string& post_data) {
            captured_requests.push_back(post_data);
          })),
      temp_dir.GetPath().Append(FILE_PATH_LITERAL("crx_cache")));

  scoped_refptr<PersistedData> persisted_data =
      configurator->GetUpdaterPersistedData();
  for (const std::string& app_id : registered_apps) {
    RegistrationRequest registration;
    registration.app_id = app_id;
    registration.version = "1.0.0.0";
    persisted_data->RegisterApp(registration);
  }
  const std::vector<std::string> persisted_app_ids =
      persisted_data->GetAppIds();
  ASSERT_EQ(persisted_app_ids.size(), 10u);

  auto service = base::MakeRefCounted<UpdateServiceImplImpl>(configurator);
  base::RunLoop loop;
  UpdateService::Result update_result = UpdateService::Result::kServiceFailed;
  service->UpdateAll(base::DoNothing(), base::BindLambdaForTesting(
                                            [&](UpdateService::Result result) {
                                              update_result = result;
                                              loop.Quit();
                                            }));
  loop.Run();
  EXPECT_EQ(update_result, UpdateService::Result::kSuccess);
  ASSERT_EQ(captured_requests.size(), 1u);

  std::optional<base::Value> doc = base::JSONReader::Read(
      captured_requests[0], base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  ASSERT_TRUE(doc);
  const base::ListValue* sent_apps =
      doc->GetDict().FindListByDottedPath("request.apps");
  ASSERT_TRUE(sent_apps);
  std::vector<std::string> sent_app_ids;
  for (const base::Value& app : *sent_apps) {
    const std::string* app_id = app.GetDict().FindString("appid");
    ASSERT_TRUE(app_id);
    sent_app_ids.push_back(*app_id);
  }
  EXPECT_THAT(sent_app_ids,
              ::testing::UnorderedElementsAreArray(persisted_app_ids));

  // A shuffle of 10 apps keeps the original order with probability
  // 1/10! (about 1 in 3.6 million).
  EXPECT_NE(sent_app_ids, persisted_app_ids);
}

class UpdateServiceImplInstallTrackingTest : public ::testing::Test {
 protected:
  void SetUp() override {
#if BUILDFLAG(IS_POSIX)
    if (IsSystemInstall(scope_)) {
      GTEST_SKIP();
    }
#endif  // BUILDFLAG(IS_POSIX)
    // An empty update URL makes network requests fail immediately.
    ASSERT_TRUE(ExternalConstantsBuilder()
                    .SetUpdateURL(std::vector<std::string>{})
                    .Overwrite());
    scoped_refptr<GlobalPrefs> global_prefs = CreateGlobalPrefs(scope_);
    ASSERT_TRUE(global_prefs);
    config_ = base::MakeRefCounted<Configurator>(
        global_prefs, CreateExternalConstants(), scope_);
  }

  void TearDown() override {
    if (IsSkipped()) {
      return;
    }
    config_ = nullptr;
    base::DeletePathRecursively(*GetInstallDirectory(scope_));
  }

  base::test::TaskEnvironment task_environment_;
  const UpdaterScope scope_ = GetUpdaterScopeForTesting();
  scoped_refptr<Configurator> config_;
};

// TODO(crbug.com/584356535): Re-enable once this stops leaking the GlobalPrefs
// lock and causing subsequent IntegrationTests in the same batch to hang.
TEST_F(UpdateServiceImplInstallTrackingTest, DISABLED_IsInstalling) {
  auto service = base::MakeRefCounted<UpdateServiceImplImpl>(config_);
  base::RunLoop run_loop;
  const auto done = base::BarrierClosure(2, run_loop.QuitClosure());
  for (const char* app_id : {"app", "APP"}) {
    service->RunInstaller(
        app_id, base::FilePath(FILE_PATH_LITERAL("nonexistent_installer")),
        /*install_args=*/{}, /*install_data=*/{}, /*install_settings=*/{},
        /*language=*/{}, base::DoNothing(),
        base::IgnoreArgs<UpdateService::Result>(done));
  }
  EXPECT_TRUE(service->IsInstalling("App"));
  run_loop.Run();
  EXPECT_FALSE(service->IsInstalling("app"));
}

TEST_F(UpdateServiceImplInstallTrackingTest,
       RemoveUninstalledAppsTaskSkipsInstallingApps) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  scoped_refptr<PersistedData> persisted_data =
      config_->GetUpdaterPersistedData();

  // Both apps look uninstalled: no `Clients` key on Windows, and a missing
  // existence checker path elsewhere.
  for (const char* app_id : {"installing", "uninstalled"}) {
    RegistrationRequest request;
    request.app_id = app_id;
    request.version = "1.0";
    request.existence_checker_path = temp_dir.GetPath().AppendASCII(app_id);
    persisted_data->RegisterApp(request);
  }

  base::RunLoop run_loop;
  base::MakeRefCounted<RemoveUninstalledAppsTask>(
      config_, scope_, base::BindRepeating([](const std::string& app_id) {
        return app_id == "installing";
      }))
      ->Run(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_TRUE(persisted_data->HasApp("installing"));
  EXPECT_FALSE(persisted_data->HasApp("uninstalled"));
  persisted_data->RemoveApp("installing");
}

#if BUILDFLAG(IS_WIN)
struct UpdateServiceImplGetInstallerTextTestCase {
  const UpdateService::ErrorCategory error_category;
  const int error_code;
  const std::string language;
  const std::string expected_completion_message;
  std::optional<int> extra_code;
};

class UpdateServiceImplGetInstallerTextTest
    : public ::testing::TestWithParam<
          UpdateServiceImplGetInstallerTextTestCase> {};

INSTANTIATE_TEST_SUITE_P(
    UpdateServiceImplGetInstallerTextTestCases,
    UpdateServiceImplGetInstallerTextTest,
    ::testing::ValuesIn(std::vector<UpdateServiceImplGetInstallerTextTestCase>{
        {UpdateService::ErrorCategory::kDownload,
         std::to_underlying(update_client::CrxDownloaderError::NO_URL),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_DOWNLOAD_ERROR_BASE,
             L"update_client::CrxDownloaderError::NO_URL"))},
        {UpdateService::ErrorCategory::kDownload,
         std::to_underlying(update_client::CrxDownloaderError::NO_HASH),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_DOWNLOAD_ERROR_BASE,
             L"update_client::CrxDownloaderError::NO_HASH"))},
        {UpdateService::ErrorCategory::kDownload,
         std::to_underlying(update_client::CrxDownloaderError::BAD_HASH),
         {},
         base::WideToUTF8(GetLocalizedString(IDS_DOWNLOAD_HASH_MISMATCH_BASE))},
        {UpdateService::ErrorCategory::kDownload,
         std::to_underlying(
             update_client::CrxDownloaderError::BITS_TOO_MANY_JOBS),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_DOWNLOAD_ERROR_BASE,
             L"update_client::CrxDownloaderError::BITS_TOO_MANY_JOBS"))},
        {UpdateService::ErrorCategory::kDownload,
         std::to_underlying(
             update_client::CrxDownloaderError::DO_TOO_MANY_DOWNLOADS),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_DOWNLOAD_ERROR_BASE,
             L"update_client::CrxDownloaderError::DO_TOO_MANY_DOWNLOADS"))},
        {UpdateService::ErrorCategory::kDownload,
         std::to_underlying(update_client::CrxDownloaderError::GENERIC_ERROR),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_DOWNLOAD_ERROR_BASE,
             L"update_client::CrxDownloaderError::GENERIC_ERROR"))},
        {UpdateService::ErrorCategory::kDownload,
         0xFFFF,
         {},
         base::WideToUTF8(GetLocalizedStringF(IDS_GENERIC_DOWNLOAD_ERROR_BASE,
                                              L"0xffff"))},
        {UpdateService::ErrorCategory::kUnpack,
         std::to_underlying(update_client::UnpackerError::kInvalidParams),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kInvalidParams"))},
        {UpdateService::ErrorCategory::kUnpack,
         std::to_underlying(update_client::UnpackerError::kInvalidFile),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kInvalidFile"))},
        {UpdateService::ErrorCategory::kUnpack,
         std::to_underlying(update_client::UnpackerError::kUnzipPathError),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kUnzipPathError"))},
        {UpdateService::ErrorCategory::kUnpack,
         std::to_underlying(update_client::UnpackerError::kUnzipFailed),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kUnzipFailed"))},
        {UpdateService::ErrorCategory::kUnpack,
         std::to_underlying(update_client::UnpackerError::kBadManifest),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kBadManifest"))},
        {UpdateService::ErrorCategory::kUnpack,
         std::to_underlying(update_client::UnpackerError::kBadExtension),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kBadExtension"))},
        {UpdateService::ErrorCategory::kUnpack,
         std::to_underlying(update_client::UnpackerError::kIoError),
         {},
         base::WideToUTF8(
             GetLocalizedStringF(IDS_GENERIC_UNPACK_ERROR_BASE,
                                 L"update_client::UnpackerError::kIoError"))},
        {UpdateService::ErrorCategory::kUnpack,
         std::to_underlying(
             update_client::UnpackerError::kDeltaVerificationFailure),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kDeltaVerificationFailure"))},
        {UpdateService::ErrorCategory::kUnpack,
         std::to_underlying(update_client::UnpackerError::kDeltaBadCommands),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kDeltaBadCommands"))},
        {UpdateService::ErrorCategory::kUnpack,
         std::to_underlying(
             update_client::UnpackerError::kDeltaUnsupportedCommand),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kDeltaUnsupportedCommand"))},
        {UpdateService::ErrorCategory::kUnpack,
         std::to_underlying(
             update_client::UnpackerError::kDeltaOperationFailure),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kDeltaOperationFailure"))},
        {UpdateService::ErrorCategory::kUnpack,
         std::to_underlying(
             update_client::UnpackerError::kDeltaPatchProcessFailure),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kDeltaPatchProcessFailure"))},
        {UpdateService::ErrorCategory::kUnpack,
         std::to_underlying(
             update_client::UnpackerError::kDeltaMissingExistingFile),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kDeltaMissingExistingFile"))},
        {UpdateService::ErrorCategory::kUnpack,
         std::to_underlying(
             update_client::UnpackerError::kPuffinMissingPreviousCrx),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kPuffinMissingPreviousCrx"))},
        {UpdateService::ErrorCategory::kUnpack,
         std::to_underlying(update_client::UnpackerError::kFailedToAddToCache),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_UNPACK_CACHING_ERROR_BASE,
             L"update_client::UnpackerError::kFailedToAddToCache"))},
        {UpdateService::ErrorCategory::kUnpack,
         std::to_underlying(
             update_client::UnpackerError::kFailedToCreateCacheDir),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_UNPACK_CACHING_ERROR_BASE,
             L"update_client::UnpackerError::kFailedToCreateCacheDir"))},
        {UpdateService::ErrorCategory::kUnpack,
         std::to_underlying(update_client::UnpackerError::kCrxCacheNotProvided),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UNPACK_ERROR_BASE,
             L"update_client::UnpackerError::kCrxCacheNotProvided"))},
        {UpdateService::ErrorCategory::kUnpack,
         0xFFFF,
         {},
         base::WideToUTF8(GetLocalizedStringF(IDS_GENERIC_UNPACK_ERROR_BASE,
                                              L"0xffff"))},
        {UpdateService::ErrorCategory::kService,
         std::to_underlying(update_client::ServiceError::SERVICE_WAIT_FAILED),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_SERVICE_ERROR_BASE,
             L"update_client::ServiceError::SERVICE_WAIT_FAILED"))},
        {UpdateService::ErrorCategory::kService,
         std::to_underlying(update_client::ServiceError::UPDATE_DISABLED),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_SERVICE_ERROR_BASE,
             L"update_client::ServiceError::UPDATE_DISABLED"))},
        {UpdateService::ErrorCategory::kService,
         std::to_underlying(update_client::ServiceError::CANCELLED),
         {},
         base::WideToUTF8(
             GetLocalizedString(IDS_SERVICE_ERROR_CANCELLED_BASE))},
        {UpdateService::ErrorCategory::kService,
         std::to_underlying(update_client::ServiceError::CHECK_FOR_UPDATE_ONLY),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_SERVICE_ERROR_BASE,
             L"update_client::ServiceError::CHECK_FOR_UPDATE_ONLY"))},
        {UpdateService::ErrorCategory::kService,
         0xFFFF,
         {},
         base::WideToUTF8(GetLocalizedStringF(IDS_GENERIC_SERVICE_ERROR_BASE,
                                              L"0xffff"))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         std::to_underlying(update_client::ProtocolError::RESPONSE_NOT_TRUSTED),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
             L"update_client::ProtocolError::RESPONSE_NOT_TRUSTED"))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         std::to_underlying(update_client::ProtocolError::MISSING_URLS),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
             L"update_client::ProtocolError::MISSING_URLS"))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         std::to_underlying(update_client::ProtocolError::PARSE_FAILED),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
             L"update_client::ProtocolError::PARSE_FAILED"))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         std::to_underlying(
             update_client::ProtocolError::UPDATE_RESPONSE_NOT_FOUND),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
             L"update_client::ProtocolError::UPDATE_RESPONSE_NOT_FOUND"))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         std::to_underlying(update_client::ProtocolError::URL_FETCHER_FAILED),
         {},
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
             L"update_client::ProtocolError::URL_FETCHER_FAILED"))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         std::to_underlying(update_client::ProtocolError::UNKNOWN_APPLICATION),
         "en",
         base::WideToUTF8(GetLocalizedString(IDS_UNKNOWN_APPLICATION_BASE))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         std::to_underlying(
             update_client::ProtocolError::RESTRICTED_APPLICATION),
         "en",
         base::WideToUTF8(
             GetLocalizedString(IDS_RESTRICTED_RESPONSE_FROM_SERVER_BASE))},
        {UpdateService::ErrorCategory::kUpdateCheck,
         std::to_underlying(update_client::ProtocolError::INVALID_APPID), "en",
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
             L"update_client::ProtocolError::INVALID_APPID"))},
        {UpdateService::ErrorCategory::kUpdateCheck, 401, "en",
         base::WideToUTF8(
             GetLocalizedString(IDS_ERROR_HTTPSTATUS_UNAUTHORIZED_BASE))},
        {UpdateService::ErrorCategory::kUpdateCheck, 401, "ar",
         base::WideToUTF8(
             GetLocalizedString(IDS_ERROR_HTTPSTATUS_UNAUTHORIZED_BASE,
                                L"ar"))},
        {UpdateService::ErrorCategory::kUpdateCheck, 403, "en",
         base::WideToUTF8(
             GetLocalizedString(IDS_ERROR_HTTPSTATUS_FORBIDDEN_BASE))},
        {UpdateService::ErrorCategory::kUpdateCheck, 407, "en",
         base::WideToUTF8(GetLocalizedString(
             IDS_ERROR_HTTPSTATUS_PROXY_AUTH_REQUIRED_BASE))},
        {UpdateService::ErrorCategory::kUpdateCheck, 404, "en",
         base::WideToUTF8(
             GetLocalizedStringF(IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
                                 L"HTTP 404"))},
        {UpdateService::ErrorCategory::kUpdateCheck, 0xFFFF, "en",
         base::WideToUTF8(
             GetLocalizedStringF(IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
                                 L"0xffff"))},
        {UpdateService::ErrorCategory::kUpdateCheck, 0xFFFF, "es",
         base::WideToUTF8(
             GetLocalizedStringF(IDS_GENERIC_UPDATE_CHECK_ERROR_BASE,
                                 L"0xffff",
                                 L"es"))},
        {UpdateService::ErrorCategory::kInstall,
         std::to_underlying(
             update_client::InstallError::FINGERPRINT_WRITE_FAILED),
         "en",
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::FINGERPRINT_WRITE_FAILED"))},
        {UpdateService::ErrorCategory::kInstall,
         std::to_underlying(update_client::InstallError::BAD_MANIFEST), "en",
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::BAD_MANIFEST"))},
        {UpdateService::ErrorCategory::kInstall,
         std::to_underlying(update_client::InstallError::GENERIC_ERROR), "en",
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::GENERIC_ERROR"))},
        {UpdateService::ErrorCategory::kInstall,
         std::to_underlying(update_client::InstallError::MOVE_FILES_ERROR),
         "en",
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::MOVE_FILES_ERROR"))},
        {UpdateService::ErrorCategory::kInstall,
         std::to_underlying(
             update_client::InstallError::SET_PERMISSIONS_FAILED),
         "en",
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::SET_PERMISSIONS_FAILED"))},
        {UpdateService::ErrorCategory::kInstall,
         std::to_underlying(update_client::InstallError::INVALID_VERSION), "en",
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::INVALID_VERSION"))},
        {UpdateService::ErrorCategory::kInstall,
         std::to_underlying(update_client::InstallError::VERSION_NOT_UPGRADED),
         "en",
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::VERSION_NOT_UPGRADED"))},
        {UpdateService::ErrorCategory::kInstall,
         std::to_underlying(update_client::InstallError::NO_DIR_COMPONENT_USER),
         "en",
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::NO_DIR_COMPONENT_USER"))},
        {UpdateService::ErrorCategory::kInstall,
         std::to_underlying(
             update_client::InstallError::CLEAN_INSTALL_DIR_FAILED),
         "en",
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::CLEAN_INSTALL_DIR_FAILED"))},
        {UpdateService::ErrorCategory::kInstall,
         std::to_underlying(
             update_client::InstallError::INSTALL_VERIFICATION_FAILED),
         "en",
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::INSTALL_VERIFICATION_FAILED"))},
        {UpdateService::ErrorCategory::kInstall,
         std::to_underlying(
             update_client::InstallError::MISSING_INSTALL_PARAMS),
         "en",
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::MISSING_INSTALL_PARAMS"))},
        {UpdateService::ErrorCategory::kInstall,
         std::to_underlying(update_client::InstallError::LAUNCH_PROCESS_FAILED),
         "en",
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::LAUNCH_PROCESS_FAILED"))},
        {UpdateService::ErrorCategory::kInstall,
         std::to_underlying(update_client::InstallError::INSTALL_PATH_ERROR),
         "en",
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::INSTALL_PATH_ERROR"))},
        {UpdateService::ErrorCategory::kInstall,
         std::to_underlying(update_client::InstallError::CUSTOM_ERROR_BASE),
         "en",
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALL_ERROR_BASE,
             L"update_client::InstallError::CUSTOM_ERROR_BASE"))},

        // `2` is also the value for
        // `update_client::InstallError::FINGERPRINT_WRITE_FAILED`, but since
        // this is coded as an "installer_error", the error will be interpreted
        // as the Windows error code for `ERROR_FILE_NOT_FOUND` instead.
        {UpdateService::ErrorCategory::kInstaller,
         2,
         "en",
         base::WideToUTF8(GetLocalizedStringF(
             IDS_GENERIC_INSTALLER_ERROR_BASE,
             L"The system cannot find the file specified. ")),
         {}},
        {UpdateService::ErrorCategory::kInstaller,
         2,
         "hi",
         "इंस्टॉलर से जुड़ी गड़बड़ी: "
         "The system cannot find the file specified. ",
         {}},
        {UpdateService::ErrorCategory::kInstaller,
         GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY,
         "en",
         base::WideToUTF8(
             GetLocalizedStringF(IDS_APP_INSTALL_DISABLED_BY_GROUP_POLICY_BASE,
                                 L"GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY")),
         {}},
        {UpdateService::ErrorCategory::kInstaller,
         GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY,
         "en",
         base::WideToUTF8(
             GetLocalizedStringF(IDS_APP_INSTALL_DISABLED_BY_GROUP_POLICY_BASE,
                                 L"GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY")),
         {}},
        {UpdateService::ErrorCategory::kInstaller,
         GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY_MANUAL,
         "en",
         base::WideToUTF8(GetLocalizedStringF(
             IDS_APP_INSTALL_DISABLED_BY_GROUP_POLICY_BASE,
             L"GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY_MANUAL")),
         {}},
        {UpdateService::ErrorCategory::kInstaller,
         GOOPDATEINSTALL_E_FILENAME_INVALID, "en",
         base::WideToUTF8(base::StrCat(
             {GetLocalizedString(IDS_INVALID_INSTALLER_FILENAME_BASE), L"\n",
              GetLocalizedStringF(IDS_EXTRA_CODE_BASE,
                                  base::UTF8ToWide(absl::StrFormat(
                                      "%#x",
                                      kErrorMissingInstallParams)))})),
         kErrorMissingInstallParams},
        {UpdateService::ErrorCategory::kInstaller,
         GOOPDATEINSTALL_E_INSTALLER_FAILED_START, "en",
         base::WideToUTF8(base::StrCat(
             {GetLocalizedString(IDS_INSTALLER_FAILED_TO_START_BASE), L"\n",
              GetLocalizedStringF(IDS_EXTRA_CODE_BASE, L"0x2")})),
         ERROR_FILE_NOT_FOUND},
        {UpdateService::ErrorCategory::kInstaller,
         GOOPDATEINSTALL_E_INSTALLER_TIMED_OUT,
         "en",
         base::WideToUTF8(GetLocalizedString(IDS_INSTALLER_TIMED_OUT_BASE)),
         {}},

        {UpdateService::ErrorCategory::kInstaller,
         ERROR_SUCCESS_REBOOT_INITIATED,
         "en",
         base::WideToUTF8(GetLocalizedStringF(
             IDS_INSTALL_REBOOT_BASE,
             GetTextForSystemError(ERROR_SUCCESS_REBOOT_INITIATED))),
         {}},
        {UpdateService::ErrorCategory::kInstaller,
         ERROR_SUCCESS_REBOOT_REQUIRED,
         "en",
         base::WideToUTF8(GetLocalizedStringF(
             IDS_INSTALL_REBOOT_BASE,
             GetTextForSystemError(ERROR_SUCCESS_REBOOT_REQUIRED))),
         {}},
        {UpdateService::ErrorCategory::kInstaller,
         ERROR_SUCCESS_RESTART_REQUIRED,
         "en",
         base::WideToUTF8(GetLocalizedStringF(
             IDS_INSTALL_REBOOT_BASE,
             GetTextForSystemError(ERROR_SUCCESS_RESTART_REQUIRED))),
         {}},
    }));

TEST_P(UpdateServiceImplGetInstallerTextTest, TestCases) {
  ASSERT_EQ(
      GetInstallerText(GetParam().error_category, GetParam().error_code,
                       GetParam().extra_code.value_or(0), GetParam().language),
      GetParam().expected_completion_message);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace updater::internal
