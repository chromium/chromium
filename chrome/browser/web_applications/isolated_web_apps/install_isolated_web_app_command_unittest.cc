// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/pending_install_info.h"
#include "chrome/browser/web_applications/isolation_data.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/mock_data_retriever.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "chrome/test/base/testing_profile.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace web_app {
namespace {

using ::base::BucketsAre;
using ::base::test::IsNotNullCallback;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Action;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsNull;
using ::testing::IsTrue;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;
using ::testing::WithArg;

IsolatedWebAppUrlInfo CreateRandomIsolatedWebAppUrlInfo() {
  web_package::SignedWebBundleId signed_web_bundle_id =
      web_package::SignedWebBundleId::CreateRandomForDevelopment();
  base::expected<IsolatedWebAppUrlInfo, std::string> url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(signed_web_bundle_id);
  if (!url_info.has_value()) {
    CHECK(false) << "Failed to create testing web app url info: "
                 << url_info.error();
  }
  return url_info.value();
}

IsolationData CreateIsolationDataDevProxy(
    base::StringPiece dev_mode_proxy_url = "http://default-proxy-url.org/") {
  return IsolationData{IsolationData::DevModeProxy{
      .proxy_url = url::Origin::Create(GURL(dev_mode_proxy_url))}};
}

blink::mojom::ManifestPtr CreateDefaultManifest(const GURL& application_url) {
  auto manifest = blink::mojom::Manifest::New();
  manifest->id = u"";
  manifest->scope = application_url.Resolve("/");
  manifest->start_url = application_url.Resolve("/testing-start-url.html");
  manifest->display = DisplayMode::kStandalone;
  manifest->short_name = u"test short manifest name";
  return manifest;
}

GURL CreateDefaultManifestURL(const GURL& application_url) {
  return application_url.Resolve("/manifest.webmanifest");
}

auto ReturnManifest(const blink::mojom::ManifestPtr& manifest,
                    const GURL& manifest_url,
                    bool is_installable = true) {
  constexpr int kCallbackArgumentIndex = 2;

  return DoAll(
      WithArg<kCallbackArgumentIndex>(
          [](const WebAppDataRetriever::CheckInstallabilityCallback& callback) {
            DCHECK(!callback.is_null());
          }),
      RunOnceCallback<kCallbackArgumentIndex>(
          /*manifest=*/manifest.Clone(),
          /*manifest_url=*/manifest_url,
          /*valid_manifest_for_web_app=*/true,
          /*is_installable=*/is_installable));
}

std::unique_ptr<MockDataRetriever> CreateDefaultDataRetriever(
    const GURL& application_url) {
  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      std::make_unique<NiceMock<MockDataRetriever>>();

  EXPECT_CALL(*fake_data_retriever, GetWebAppInstallInfo).Times(0);

  ON_CALL(*fake_data_retriever, CheckInstallabilityAndRetrieveManifest)
      .WillByDefault(ReturnManifest(CreateDefaultManifest(application_url),
                                    CreateDefaultManifestURL(application_url)));

  std::map<GURL, std::vector<SkBitmap>> icons = {};

  using HttpStatusCode = int;
  std::map<GURL, HttpStatusCode> http_result = {};

  ON_CALL(*fake_data_retriever, GetIcons(_, _, _, IsNotNullCallback()))
      .WillByDefault(RunOnceCallback<3>(IconsDownloadedResult::kCompleted,
                                        std::move(icons), http_result));

  return fake_data_retriever;
}

class InstallIsolatedWebAppCommandTest : public ::testing::Test {
 public:
  void SetUp() override {
    FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile());

    auto command_manager_url_loader = std::make_unique<TestWebAppUrlLoader>();
    command_manager_url_loader->SetPrepareForLoadResultLoaded();
    provider->GetCommandManager().SetUrlLoaderForTesting(
        std::move(command_manager_url_loader));

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  WebAppProvider& web_app_provider() {
    auto* web_app_provider = WebAppProvider::GetForTest(profile());
    DCHECK(web_app_provider != nullptr);
    return *web_app_provider;
  }

  WebAppRegistrar& web_app_registrar() {
    return web_app_provider().registrar_unsafe();
  }

  WebAppIconManager& web_app_icon_manager() {
    return web_app_provider().icon_manager();
  }

  WebAppCommandManager& command_manager() {
    return web_app_provider().command_manager();
  }

  void ScheduleCommand(std::unique_ptr<WebAppCommand> command) {
    command_manager().ScheduleCommand(std::move(command));
  }

  struct Parameters {
    IsolatedWebAppUrlInfo url_info;
    std::unique_ptr<WebAppUrlLoader> url_loader;
    std::unique_ptr<content::WebContents> web_contents;
    absl::optional<IsolationData> isolation_data;
    raw_ptr<WebAppInstallFinalizer> install_finalizer = nullptr;
  };

  base::expected<InstallIsolatedWebAppCommandSuccess,
                 InstallIsolatedWebAppCommandError>
  ExecuteCommand(
      Parameters parameters,
      std::unique_ptr<WebAppDataRetriever> data_retriever = nullptr) {
    base::test::TestFuture<base::expected<InstallIsolatedWebAppCommandSuccess,
                                          InstallIsolatedWebAppCommandError>>
        test_future;

    std::unique_ptr<content::WebContents> web_contents =
        std::move(parameters.web_contents);
    if (web_contents == nullptr) {
      web_contents = content::WebContents::Create(
          content::WebContents::CreateParams(profile()));
    }

    std::unique_ptr<WebAppUrlLoader> url_loader =
        std::move(parameters.url_loader);
    if (url_loader == nullptr) {
      auto test_url_loader = std::make_unique<TestWebAppUrlLoader>();
      test_url_loader->SetNextLoadUrlResult(
          parameters.url_info.origin().GetURL().Resolve(
              ".well-known/_generated_install_page.html"),
          WebAppUrlLoader::Result::kUrlLoaded);

      url_loader = std::move(test_url_loader);
    }

    auto command = CreateCommand(
        parameters.url_info, std::move(web_contents), parameters.isolation_data,
        std::move(url_loader), test_future.GetCallback());

    command->SetDataRetrieverForTesting(
        data_retriever != nullptr ? std::move(data_retriever)
                                  : CreateDefaultDataRetriever(
                                        parameters.url_info.origin().GetURL()));
    ScheduleCommand(std::move(command));
    return test_future.Get();
  }

  std::unique_ptr<InstallIsolatedWebAppCommand> CreateCommand(
      const IsolatedWebAppUrlInfo& url_info,
      std::unique_ptr<content::WebContents> web_contents,
      absl::optional<IsolationData> isolation_data,
      std::unique_ptr<WebAppUrlLoader> url_loader,
      base::OnceCallback<
          void(base::expected<InstallIsolatedWebAppCommandSuccess,
                              InstallIsolatedWebAppCommandError>)> callback) {
    if (!isolation_data.has_value()) {
      isolation_data = CreateIsolationDataDevProxy();
    }

    return std::make_unique<InstallIsolatedWebAppCommand>(
        url_info, isolation_data.value(), std::move(web_contents),
        std::move(url_loader), *profile(), std::move(callback));
  }

  base::expected<InstallIsolatedWebAppCommandSuccess,
                 InstallIsolatedWebAppCommandError>
  ExecuteCommandWithManifest(const IsolatedWebAppUrlInfo& url_info,
                             const blink::mojom::ManifestPtr& manifest,
                             absl::optional<IsolationData> isolation_data =
                                 absl::optional<IsolationData>()) {
    GURL application_url = url_info.origin().GetURL();
    std::unique_ptr<MockDataRetriever> fake_data_retriever =
        CreateDefaultDataRetriever(application_url);

    ON_CALL(*fake_data_retriever, CheckInstallabilityAndRetrieveManifest)
        .WillByDefault(ReturnManifest(
            manifest, CreateDefaultManifestURL(application_url)));

    return ExecuteCommand(
        Parameters{
            .url_info = url_info,
            .isolation_data = isolation_data,
        },
        std::move(fake_data_retriever));
  }

  TestingProfile* profile() const { return profile_.get(); }

 private:
  // Task environment allow to |base::OnceCallback| work in unit test.
  //
  // See details in //docs/threading_and_tasks_testing.md.
  content::BrowserTaskEnvironment browser_task_environment_;

  std::unique_ptr<TestingProfile> profile_ = []() {
    TestingProfile::Builder builder;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    builder.SetIsMainProfile(true);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

    return builder.Build();
  }();
};

MATCHER_P(IsExpectedValue, value_matcher, "") {
  if (!arg.has_value()) {
    *result_listener << "which is not engaged";
    return false;
  }

  return ExplainMatchResult(value_matcher, arg.value(), result_listener);
}

MATCHER_P(IsUnexpectedValue, error_matcher, "") {
  if (arg.has_value()) {
    *result_listener << "which is not engaged";
    return false;
  }

  return ExplainMatchResult(error_matcher, arg.error(), result_listener);
}

MATCHER(IsInstallationOk, "") {
  bool result = ExplainMatchResult(IsExpectedValue(_), arg, result_listener);
  if (!result) {
    DCHECK(!arg.has_value());
    *result_listener << ", error: " << arg.error();
  }

  return result;
}

MATCHER_P(IsInstallationError, message_matcher, "") {
  return ExplainMatchResult(
      IsUnexpectedValue(ResultOf(
          "error.message",
          [](const InstallIsolatedWebAppCommandError& error) {
            return error.message;
          },
          message_matcher)),
      arg, result_listener);
}

MATCHER(IsInstallationError, "") {
  return ExplainMatchResult(IsUnexpectedValue(_), arg, result_listener);
}

TEST_F(InstallIsolatedWebAppCommandTest,
       ServiceWorkerIsNotRequiredForInstallation) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever(url_info.origin().GetURL());

  EXPECT_CALL(*fake_data_retriever,
              CheckInstallabilityAndRetrieveManifest(
                  _, /*bypass_service_worker_check=*/IsTrue(), _, _))
      .WillOnce(ReturnManifest(
          // IsolatedWebAppUrlLoaderFactory is responsible for resolving
          // isolated-app:// schema requests.
          CreateDefaultManifest(url_info.origin().GetURL()),
          CreateDefaultManifestURL(url_info.origin().GetURL())));

  EXPECT_THAT(ExecuteCommand(
                  Parameters{
                      .url_info = url_info,
                  },
                  std::move(fake_data_retriever)),
              IsInstallationOk());
}

TEST_F(InstallIsolatedWebAppCommandTest, PropagateErrorWhenURLLoaderFails) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(
      url_info.origin().GetURL().Resolve(
          ".well-known/_generated_install_page.html"),
      WebAppUrlLoader::Result::kFailedErrorPageLoaded);

  EXPECT_THAT(ExecuteCommand(Parameters{
                  .url_info = url_info,
                  .url_loader = std::move(url_loader),
              }),
              IsInstallationError(HasSubstr("Error during URL loading: ")));
}

TEST_F(InstallIsolatedWebAppCommandTest,
       PropagateErrorWhenURLLoaderFailsWithDestroyedWebContentsError) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(
      url_info.origin().GetURL().Resolve(
          ".well-known/_generated_install_page.html"),
      WebAppUrlLoaderResult::kFailedWebContentsDestroyed);

  EXPECT_THAT(ExecuteCommand(Parameters{
                  .url_info = url_info,
                  .url_loader = std::move(url_loader),
              }),
              IsInstallationError(HasSubstr(
                  "Error during URL loading: FailedWebContentsDestroyed")));
}

TEST_F(InstallIsolatedWebAppCommandTest,
       URLLoaderIsCalledWithURLgivenToTheInstallCommand) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();

  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(
      url_info.origin().GetURL().Resolve(
          ".well-known/_generated_install_page.html"),
      WebAppUrlLoader::Result::kUrlLoaded);

  EXPECT_THAT(ExecuteCommand(Parameters{
                  .url_info = url_info,
                  .url_loader = std::move(url_loader),
              }),
              IsInstallationOk());
}

TEST_F(InstallIsolatedWebAppCommandTest, URLLoaderIgnoresQueryParameters) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(
      url_info.origin().GetURL().Resolve(
          ".well-known/_generated_install_page.html"),
      WebAppUrlLoader::Result::kUrlLoaded);

  absl::optional<WebAppUrlLoader::UrlComparison> last_url_comparison =
      absl::nullopt;
  url_loader->TrackLoadUrlCalls(base::BindLambdaForTesting(
      [&](const GURL& unused_url, content::WebContents* unused_web_contents,
          WebAppUrlLoader::UrlComparison url_comparison) {
        last_url_comparison = url_comparison;
      }));

  EXPECT_THAT(ExecuteCommand(Parameters{
                  .url_info = url_info,
                  .url_loader = std::move(url_loader),
              }),
              IsInstallationOk());

  EXPECT_THAT(
      last_url_comparison,
      Optional(Eq(WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef)));
}

TEST_F(InstallIsolatedWebAppCommandTest,
       InstallationSucceedesWhenFinalizerReturnSuccessNewInstall) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();

  EXPECT_THAT(ExecuteCommand(Parameters{
                  .url_info = url_info,
              }),
              IsInstallationOk());
}

TEST_F(InstallIsolatedWebAppCommandTest,
       InstallationFinalizedWithIsolatedWebAppDevInstallInstallSource) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever(url_info.origin().GetURL());

  EXPECT_THAT(ExecuteCommand(
                  Parameters{
                      .url_info = url_info,
                  },
                  std::move(fake_data_retriever)),
              IsInstallationOk());

  using InstallSource = webapps::WebappInstallSource;

  const WebApp* web_app = web_app_registrar().GetAppById(url_info.app_id());
  ASSERT_THAT(web_app, NotNull());

  EXPECT_THAT(web_app->GetSources().test(WebAppManagement::kCommandLine),
              IsTrue());

  EXPECT_THAT(web_app->install_source_for_metrics(),
              Optional(Eq(InstallSource::ISOLATED_APP_DEV_INSTALL)));
}

TEST_F(InstallIsolatedWebAppCommandTest,
       InstallationFailsWhenAppIsNotInstallable) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever(url_info.origin().GetURL());

  ON_CALL(*fake_data_retriever, CheckInstallabilityAndRetrieveManifest)
      .WillByDefault(
          ReturnManifest(blink::mojom::Manifest::New(),
                         GURL{"http://test-url-example.com/manifest.json"},
                         /*is_installable=*/false));

  EXPECT_THAT(ExecuteCommand(
                  Parameters{
                      .url_info = url_info,
                  },
                  std::move(fake_data_retriever)),
              IsInstallationError(HasSubstr("App is not installable")));
}

TEST_F(InstallIsolatedWebAppCommandTest, CommandLocksOnAppIdAndWebContents) {
  base::test::TestFuture<base::expected<InstallIsolatedWebAppCommandSuccess,
                                        InstallIsolatedWebAppCommandError>>
      test_future;

  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto command = CreateCommand(
      url_info,
      content::WebContents::Create(
          content::WebContents::CreateParams(profile())),
      CreateIsolationDataDevProxy(), std::make_unique<TestWebAppUrlLoader>(),
      test_future.GetCallback());
  EXPECT_THAT(
      command->lock_description(),
      AllOf(Property(&LockDescription::type, Eq(LockDescription::Type::kApp)),
            Property(&LockDescription::app_ids,
                     UnorderedElementsAre(url_info.app_id()))));
}

TEST_F(InstallIsolatedWebAppCommandTest,
       InstallationFailsWhenAppIsInstallableButManifestIsNull) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever(url_info.origin().GetURL());

  ON_CALL(*fake_data_retriever, CheckInstallabilityAndRetrieveManifest)
      .WillByDefault(ReturnManifest(
          /*manifest=*/nullptr,
          CreateDefaultManifestURL(url_info.origin().GetURL())));

  EXPECT_THAT(ExecuteCommand(
                  Parameters{
                      .url_info = url_info,
                  },
                  std::move(fake_data_retriever)),
              IsInstallationError(HasSubstr("Manifest is null")));
}

TEST_F(InstallIsolatedWebAppCommandTest, IsolationDataSentToFinalizer) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();

  EXPECT_THAT(ExecuteCommand(Parameters{
                  .url_info = url_info,
                  .isolation_data =
                      IsolationData{
                          IsolationData::DevModeProxy{
                              .proxy_url = url::Origin::Create(
                                  GURL("http://some-testing-proxy-url.com/"))},
                      },
              }),
              IsInstallationOk());

  EXPECT_THAT(web_app_registrar().GetAppById(url_info.app_id()),
              Pointee(AllOf(Property(
                  "isolation_data", &WebApp::isolation_data,
                  Optional(Field(
                      "content", &IsolationData::content,
                      VariantWith<IsolationData::DevModeProxy>(Field(
                          "proxy_url", &IsolationData::DevModeProxy::proxy_url,
                          Eq(url::Origin::Create(GURL(
                              "http://some-testing-proxy-url.com/")))))))))));
}

TEST_F(InstallIsolatedWebAppCommandTest,
       CreatesStorageParitionDuringInstallation) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(
      url_info.origin().GetURL().Resolve(
          ".well-known/_generated_install_page.html"),
      WebAppUrlLoader::Result::kUrlLoaded);

  EXPECT_THAT(ExecuteCommand(Parameters{.url_info = url_info,
                                        .url_loader = std::move(url_loader)}),
              IsInstallationOk());

  EXPECT_THAT(profile()->GetStoragePartition(
                  url_info.storage_partition_config(profile()),
                  /*can_create=*/false),
              NotNull());
}

TEST_F(InstallIsolatedWebAppCommandTest, UsersCanDeleteIsolatedApp) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  ASSERT_THAT(ExecuteCommand(Parameters{
                  .url_info = url_info,
              }),
              IsInstallationOk());

  EXPECT_THAT(web_app_registrar().GetAppById(url_info.app_id()),
              Pointee(Property("CanUserUninstallWebApp",
                               &WebApp::CanUserUninstallWebApp, IsTrue())));
}

TEST_F(InstallIsolatedWebAppCommandTest,
       CreatesStoragePartitionBeforeUrlLoading) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(
      url_info.origin().GetURL().Resolve(
          ".well-known/_generated_install_page.html"),
      WebAppUrlLoader::Result::kUrlLoaded);

  content::StoragePartition* storage_partition_during_url_loading = nullptr;
  url_loader->TrackLoadUrlCalls(base::BindLambdaForTesting(
      [&](const GURL& unused_url, content::WebContents* unused_web_contents,
          WebAppUrlLoader::UrlComparison unused_url_comparison) {
        storage_partition_during_url_loading = profile()->GetStoragePartition(
            url_info.storage_partition_config(profile()),
            /*can_create=*/false);
      }));

  EXPECT_THAT(profile()->GetStoragePartition(
                  url_info.storage_partition_config(profile()),
                  /*can_create=*/false),
              IsNull());

  EXPECT_THAT(ExecuteCommand({
                  .url_info = url_info,
                  .url_loader = std::move(url_loader),
              }),
              IsInstallationOk());

  EXPECT_THAT(storage_partition_during_url_loading, NotNull());
}

using InstallIsolatedWebAppCommandManifestTest =
    InstallIsolatedWebAppCommandTest;

TEST_F(InstallIsolatedWebAppCommandManifestTest,
       InstallationFailsWhenManifestHasNoId) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest(url_info.origin().GetURL());
  manifest->id = absl::nullopt;

  EXPECT_THAT(
      ExecuteCommandWithManifest(url_info, manifest.Clone()),

      IsInstallationError(HasSubstr(
          "Manifest `id` is not present. manifest_url: " +
          CreateDefaultManifestURL(url_info.origin().GetURL()).spec())));

  EXPECT_THAT(web_app_registrar().GetAppById(url_info.app_id()), IsNull());
}

TEST_F(InstallIsolatedWebAppCommandManifestTest,
       FailsWhenManifestIdHasInvalidUTF8Character) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest(url_info.origin().GetURL());
  char16_t invalid_utf8_chars = {0xD801};
  manifest->id = std::u16string{invalid_utf8_chars};

  EXPECT_THAT(ExecuteCommandWithManifest(url_info, manifest.Clone()),
              IsInstallationError(HasSubstr(
                  "Failed to convert manifest `id` from UTF16 to UTF8")));
}

TEST_F(InstallIsolatedWebAppCommandManifestTest,
       PassesManifestIdToFinalizerWhenManifestIdIsEmpty) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest(url_info.origin().GetURL());
  manifest->id = u"";

  EXPECT_THAT(ExecuteCommandWithManifest(url_info, manifest.Clone()),
              IsInstallationOk());

  EXPECT_THAT(web_app_registrar().GetAppById(url_info.app_id()), NotNull());
}

TEST_F(InstallIsolatedWebAppCommandManifestTest,
       FailsWhenManifestIdIsNotEmpty) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest(url_info.origin().GetURL());
  manifest->id = u"test-manifest-id";

  EXPECT_THAT(ExecuteCommandWithManifest(url_info, manifest.Clone()),
              IsInstallationError(HasSubstr(R"(Manifest `id` must be "/")")));

  EXPECT_THAT(web_app_registrar().GetAppById(url_info.app_id()), IsNull());
}

TEST_F(InstallIsolatedWebAppCommandManifestTest,
       FailsWhenManifestScopeIsNotSlash) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest(url_info.origin().GetURL());

  manifest->scope = url_info.origin().GetURL().Resolve("/scope");

  EXPECT_THAT(
      ExecuteCommandWithManifest(url_info, manifest.Clone()),
      IsInstallationError(HasSubstr("Scope should resolve to the origin")));

  EXPECT_THAT(web_app_registrar().GetAppById(url_info.app_id()), IsNull());
}

TEST_F(InstallIsolatedWebAppCommandManifestTest,
       InstalledApplicationScopeIsResolvedToRootWhenManifestScopeIsSlash) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest(url_info.origin().GetURL());
  manifest->scope = url_info.origin().GetURL().Resolve("/");

  EXPECT_THAT(ExecuteCommandWithManifest(url_info, manifest.Clone()),
              IsInstallationOk());

  EXPECT_THAT(web_app_registrar().GetAppById(url_info.app_id()),
              Pointee(Property("scope", &WebApp::scope,
                               Eq(url_info.origin().GetURL()))));
}

TEST_F(InstallIsolatedWebAppCommandManifestTest,
       PassesManifestNameAsUntranslatedName) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest(url_info.origin().GetURL());
  manifest->name = u"test application name";

  EXPECT_THAT(ExecuteCommandWithManifest(url_info, manifest.Clone()),
              IsInstallationOk());

  EXPECT_THAT(web_app_registrar().GetAppById(url_info.app_id()),
              Pointee(Property("untranslated_name", &WebApp::untranslated_name,
                               Eq("test application name"))));
}

TEST_F(InstallIsolatedWebAppCommandManifestTest,
       UseShortNameAsUntranslatedNameWhenNameIsNotPresent) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();

  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest(url_info.origin().GetURL());
  manifest->name = absl::nullopt;
  manifest->short_name = u"test short name";

  EXPECT_THAT(ExecuteCommandWithManifest(url_info, manifest.Clone()),
              IsInstallationOk());

  EXPECT_THAT(web_app_registrar().GetAppById(url_info.app_id()),
              Pointee(Property("untranslated_name", &WebApp::untranslated_name,
                               Eq("test short name"))));
}

TEST_F(InstallIsolatedWebAppCommandManifestTest,
       UseShortNameAsTitleWhenManifestNameIsEmpty) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest(url_info.origin().GetURL());
  manifest->name = u"";
  manifest->short_name = u"other test short name";

  EXPECT_THAT(ExecuteCommandWithManifest(url_info, manifest.Clone()),
              IsInstallationOk());

  EXPECT_THAT(web_app_registrar().GetAppById(url_info.app_id()),
              Pointee(Property("untranslated_name", &WebApp::untranslated_name,
                               Eq("other test short name"))));
}

// TODO(kuragin): Add verification for presence of application name. It should
// be title or short name in the manifest.
//
// The test crashes because |SetWebAppManifestFields| from
// web_app_install_utils.cc has DCHECK which verifies the title is not empty.
//
// After discussion with Alan Cutter, the decision is to add an additional
// valiation for app name presence inside of the command and
TEST_F(InstallIsolatedWebAppCommandManifestTest,
       DISABLED_UntranslatedNameIsEmptyWhenNameAndShortNameAreNotPresent) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest(url_info.origin().GetURL());
  manifest->name = absl::nullopt;
  manifest->short_name = absl::nullopt;

  EXPECT_THAT(ExecuteCommandWithManifest(url_info, manifest.Clone()),
              IsInstallationOk());

  EXPECT_THAT(web_app_registrar().GetAppById(url_info.app_id()),
              Pointee(Property("untranslated_name", &WebApp::untranslated_name,
                               IsEmpty())));
}

class InstallIsolatedWebAppCommandManifestIconsTest
    : public InstallIsolatedWebAppCommandManifestTest {
 public:
 protected:
  GURL kSomeTestApplicationUrl = GURL("http://manifest-test-url.com");
  void SetUp() override { InstallIsolatedWebAppCommandManifestTest::SetUp(); }

  blink::mojom::ManifestPtr CreateManifest() const {
    return CreateDefaultManifest(kSomeTestApplicationUrl);
  }

  std::unique_ptr<MockDataRetriever> CreateFakeDataRetriever(
      blink::mojom::ManifestPtr manifest) const {
    std::unique_ptr<MockDataRetriever> fake_data_retriever =
        CreateDefaultDataRetriever(kSomeTestApplicationUrl);

    EXPECT_CALL(*fake_data_retriever, GetWebAppInstallInfo).Times(0);

    ON_CALL(*fake_data_retriever, CheckInstallabilityAndRetrieveManifest)
        .WillByDefault(ReturnManifest(
            manifest, CreateDefaultManifestURL(kSomeTestApplicationUrl)));

    return fake_data_retriever;
  }
};

constexpr int kImageSize = 96;

SkBitmap CreateTestBitmap(SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kImageSize, kImageSize);
  bitmap.eraseColor(color);
  return bitmap;
}

blink::Manifest::ImageResource CreateImageResourceForAnyPurpose(
    const GURL& image_src) {
  blink::Manifest::ImageResource image;
  image.type = u"image/png";
  image.sizes.push_back(gfx::Size{kImageSize, kImageSize});
  image.purpose = {
      blink::mojom::ManifestImageResource_Purpose::ANY,
  };
  image.src = image_src;
  return image;
}

TEST_F(InstallIsolatedWebAppCommandManifestIconsTest,
       ManifestIconIsDownloaded) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  kSomeTestApplicationUrl = url_info.origin().GetURL();
  GURL img_url = url_info.origin().GetURL().Resolve("icon.png");

  blink::mojom::ManifestPtr manifest = CreateManifest();

  manifest->icons = {CreateImageResourceForAnyPurpose(img_url)};

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateFakeDataRetriever(manifest.Clone());

  ON_CALL(*fake_data_retriever, CheckInstallabilityAndRetrieveManifest)
      .WillByDefault(ReturnManifest(
          manifest, CreateDefaultManifestURL(kSomeTestApplicationUrl)));

  std::map<GURL, std::vector<SkBitmap>> icons = {{
      img_url,
      {CreateTestBitmap(SK_ColorRED)},
  }};

  using HttpStatusCode = int;
  std::map<GURL, HttpStatusCode> http_result = {
      {img_url, net::HttpStatusCode::HTTP_OK},
  };

  EXPECT_CALL(*fake_data_retriever,
              GetIcons(_, UnorderedElementsAre(img_url),
                       /*skip_page_favicons=*/true, IsNotNullCallback()))
      .WillOnce(RunOnceCallback<3>(IconsDownloadedResult::kCompleted,
                                   std::move(icons), http_result));

  EXPECT_THAT(ExecuteCommand(
                  Parameters{
                      .url_info = url_info,
                  },
                  std::move(fake_data_retriever)),
              IsInstallationOk());

  base::test::TestFuture<std::map<SquareSizePx, SkBitmap>> test_future;
  web_app_icon_manager().ReadIconAndResize(url_info.app_id(), IconPurpose::ANY,
                                           SquareSizePx{1},
                                           test_future.GetCallback());

  std::map<SquareSizePx, SkBitmap> icon_bitmaps = test_future.Get();

  EXPECT_THAT(icon_bitmaps,
              UnorderedElementsAre(Pair(_, ResultOf(
                                               "bitmap.color.at.0.0",
                                               [](const SkBitmap& bitmap) {
                                                 return bitmap.getColor(0, 0);
                                               },
                                               Eq(SK_ColorRED)))));

  EXPECT_THAT(web_app_registrar().GetAppById(url_info.app_id()),
              Pointee(Property("manifest_icons", &WebApp::manifest_icons,
                               UnorderedElementsAre(_))));
}

TEST_F(InstallIsolatedWebAppCommandManifestIconsTest,
       InstallationFailsWhenIconDownloadingFails) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  kSomeTestApplicationUrl = url_info.origin().GetURL();
  GURL img_url = url_info.origin().GetURL().Resolve("icon.png");

  blink::mojom::ManifestPtr manifest = CreateManifest();

  manifest->icons = {CreateImageResourceForAnyPurpose(img_url)};

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateFakeDataRetriever(manifest.Clone());

  ON_CALL(*fake_data_retriever, CheckInstallabilityAndRetrieveManifest)
      .WillByDefault(ReturnManifest(
          manifest, CreateDefaultManifestURL(kSomeTestApplicationUrl)));

  std::map<GURL, std::vector<SkBitmap>> icons = {};

  using HttpStatusCode = int;
  std::map<GURL, HttpStatusCode> http_result = {};

  EXPECT_CALL(*fake_data_retriever, GetIcons(_, _, _, IsNotNullCallback()))
      .WillOnce(RunOnceCallback<3>(IconsDownloadedResult::kAbortedDueToFailure,
                                   std::move(icons), http_result));

  EXPECT_THAT(ExecuteCommand(
                  Parameters{
                      .url_info = url_info,
                  },
                  std::move(fake_data_retriever)),
              IsInstallationError(HasSubstr(
                  "Error during icon downloading: AbortedDueToFailure")));
}

TEST_F(InstallIsolatedWebAppCommandTest,
       SetDevModeIsolationDataBeforeUrlLoading) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(
      url_info.origin().GetURL().Resolve(
          ".well-known/_generated_install_page.html"),
      WebAppUrlLoader::Result::kUrlLoaded);

  absl::optional<IsolationData> isolation_data = absl::nullopt;
  url_loader->TrackLoadUrlCalls(base::BindLambdaForTesting(
      [&](const GURL& unused_url, content::WebContents* web_contents,
          WebAppUrlLoader::UrlComparison unused_url_comparison) {
        isolation_data =
            IsolatedWebAppPendingInstallInfo::FromWebContents(*web_contents)
                .isolation_data();
      }));

  EXPECT_THAT(ExecuteCommand(Parameters{
                  .url_info = url_info,
                  .url_loader = std::move(url_loader),
                  .isolation_data =
                      IsolationData{
                          IsolationData::DevModeProxy{
                              .proxy_url = url::Origin::Create(
                                  GURL("http://some-testing-proxy-url.com/"))},
                      },
              }),
              IsInstallationOk());

  EXPECT_THAT(
      isolation_data,
      Optional(Field("content", &IsolationData::content,
                     VariantWith<IsolationData::DevModeProxy>(Field(
                         "proxy_url", &IsolationData::DevModeProxy::proxy_url,
                         Eq(url::Origin::Create(
                             GURL("http://some-testing-proxy-url.com/"))))))));
}

TEST_F(InstallIsolatedWebAppCommandTest,
       SetInstalledBundleIsolationDataBeforeUrlLoading) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(
      url_info.origin().GetURL().Resolve(
          ".well-known/_generated_install_page.html"),
      WebAppUrlLoader::Result::kUrlLoaded);

  absl::optional<IsolationData> isolation_data = absl::nullopt;
  url_loader->TrackLoadUrlCalls(base::BindLambdaForTesting(
      [&](const GURL& unused_url, content::WebContents* web_contents,
          WebAppUrlLoader::UrlComparison unused_url_comparison) {
        isolation_data =
            IsolatedWebAppPendingInstallInfo::FromWebContents(*web_contents)
                .isolation_data();
      }));

  EXPECT_THAT(ExecuteCommand(Parameters{
                  .url_info = url_info,
                  .url_loader = std::move(url_loader),
                  .isolation_data =
                      IsolationData{
                          IsolationData::InstalledBundle{
                              .path = base::FilePath{FILE_PATH_LITERAL(
                                  "/testing/path/to/a/bundle")},
                          },
                      },
              }),
              IsInstallationOk());

  EXPECT_THAT(isolation_data,
              Optional(Field("content", &IsolationData::content,
                             VariantWith<IsolationData::InstalledBundle>(Field(
                                 "path", &IsolationData::InstalledBundle::path,
                                 Eq(base::FilePath{FILE_PATH_LITERAL(
                                     "/testing/path/to/a/bundle")}))))));
}

using InstallIsolatedWebAppCommandMetricsTest =
    InstallIsolatedWebAppCommandTest;

TEST_F(InstallIsolatedWebAppCommandMetricsTest,
       ReportSuccessWhenFinishedSuccessfully) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommand(Parameters{
                  .url_info = url_info,
              }),
              IsInstallationOk());

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Install.Result"),
              BucketsAre(base::Bucket(true, 1)));
}

TEST_F(InstallIsolatedWebAppCommandMetricsTest, ReportErrorWhenUrlLoaderFails) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(
      url_info.origin().GetURL().Resolve(
          ".well-known/_generated_install_page.html"),
      WebAppUrlLoader::Result::kFailedErrorPageLoaded);

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommand(Parameters{
                  .url_info = url_info,
                  .url_loader = std::move(url_loader),
              }),
              IsInstallationError());

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Install.Result"),
              BucketsAre(base::Bucket(false, 1)));
}

TEST_F(InstallIsolatedWebAppCommandMetricsTest,
       ReportFailureWhenAppIsNotInstallable) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever(url_info.origin().GetURL());

  ON_CALL(*fake_data_retriever, CheckInstallabilityAndRetrieveManifest)
      .WillByDefault(
          ReturnManifest(blink::mojom::Manifest::New(),
                         GURL{"http://test-url-example.com/manifest.json"},
                         /*is_installable=*/false));

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommand(
                  Parameters{
                      .url_info = url_info,
                  },
                  std::move(fake_data_retriever)),
              IsInstallationError());

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Install.Result"),
              BucketsAre(base::Bucket(false, 1)));
}

TEST_F(InstallIsolatedWebAppCommandMetricsTest,
       ReportFailureWhenManifestIsNull) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever(url_info.origin().GetURL());

  ON_CALL(*fake_data_retriever, CheckInstallabilityAndRetrieveManifest)
      .WillByDefault(ReturnManifest(
          /*manifest=*/nullptr,
          CreateDefaultManifestURL(url_info.origin().GetURL()),
          /*is_installable=*/false));

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommand(
                  Parameters{
                      .url_info = url_info,
                  },
                  std::move(fake_data_retriever)),
              IsInstallationError());

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Install.Result"),
              BucketsAre(base::Bucket(false, 1)));
}

TEST_F(InstallIsolatedWebAppCommandMetricsTest,
       ReportFailureWhenManifestIdIsNotEmpty) {
  IsolatedWebAppUrlInfo url_info = CreateRandomIsolatedWebAppUrlInfo();
  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest(url_info.origin().GetURL());
  manifest->id = u"test manifest id";

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommandWithManifest(url_info, manifest.Clone()),
              IsInstallationError());
  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Install.Result"),
              BucketsAre(base::Bucket(false, 1)));
}

}  // namespace
}  // namespace web_app
