// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_isolated_app_command.h"

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
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
#include "chrome/browser/web_applications/isolated_web_apps/pending_install_info.h"
#include "chrome/browser/web_applications/isolation_data.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/test/fake_install_finalizer.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/mock_data_retriever.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "chrome/test/base/testing_profile.h"
#include "components/webapps/browser/install_result_code.h"
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
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::ResultOf;
using ::testing::UnorderedElementsAre;
using ::testing::VariantWith;
using ::testing::WithArg;

IsolationData CreateDefaultIsolationData(
    base::StringPiece dev_mode_proxy_url = "http://default-proxy-url.org/") {
  return IsolationData{IsolationData::DevModeProxy{
      .proxy_url = std::string(dev_mode_proxy_url)}};
}

blink::mojom::ManifestPtr CreateDefaultManifest(
    base::StringPiece application_url) {
  auto manifest = blink::mojom::Manifest::New();
  manifest->id = u"";
  manifest->scope = GURL{application_url}.Resolve("/");
  manifest->start_url =
      GURL{application_url}.Resolve("/testing-start-url.html");
  manifest->display = DisplayMode::kStandalone;
  manifest->short_name = u"test short manifest name";
  return manifest;
}

GURL CreateDefaultManifestURL(base::StringPiece application_url) {
  return GURL{application_url}.Resolve("/manifest.webmanifest");
}

auto ReturnManifest(const blink::mojom::ManifestPtr& manifest,
                    GURL manifest_url,
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
    base::StringPiece application_url) {
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

class InstallIsolatedAppCommandTest : public ::testing::Test {
 public:
  void SetUp() override {
    FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile());
    provider->SetDefaultFakeSubsystems();
    provider->SetRunSubsystemStartupTasks(true);

    auto command_manager_url_loader = std::make_unique<TestWebAppUrlLoader>();
    command_manager_url_loader->SetPrepareForLoadResultLoaded();
    provider->GetCommandManager().SetUrlLoaderForTesting(
        std::move(command_manager_url_loader));

    auto install_finalizer = std::make_unique<FakeInstallFinalizer>();
    install_finalizer_ = install_finalizer.get();
    provider->SetInstallFinalizer(std::move(install_finalizer));

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  WebAppProvider& web_app_provider() {
    auto* web_app_provider = WebAppProvider::GetForTest(profile());
    DCHECK(web_app_provider != nullptr);
    return *web_app_provider;
  }

  WebAppCommandManager& command_manager() {
    return web_app_provider().command_manager();
  }

  void ScheduleCommand(std::unique_ptr<WebAppCommand> command) {
    command_manager().ScheduleCommand(std::move(command));
  }

  struct Parameters {
    std::string url;
    std::unique_ptr<WebAppUrlLoader> url_loader;
    std::unique_ptr<content::WebContents> web_contents;
    absl::optional<IsolationData> isolation_data;
  };

  base::expected<InstallIsolatedAppCommandSuccess,
                 InstallIsolatedAppCommandError>
  ExecuteCommand(
      Parameters parameters,
      std::unique_ptr<WebAppDataRetriever> data_retriever = nullptr) {
    base::test::TestFuture<base::expected<InstallIsolatedAppCommandSuccess,
                                          InstallIsolatedAppCommandError>>
        test_future;

    std::unique_ptr<content::WebContents> web_contents =
        std::move(parameters.web_contents);
    if (web_contents == nullptr) {
      web_contents = content::WebContents::Create(
          content::WebContents::CreateParams(profile()));
    }

    auto command = CreateCommand(
        parameters.url, std::move(web_contents),
        parameters.isolation_data.value_or(
            CreateDefaultIsolationData(parameters.url)),
        std::move(parameters.url_loader), test_future.GetCallback());
    command->SetDataRetrieverForTesting(
        data_retriever != nullptr ? std::move(data_retriever)
                                  : CreateDefaultDataRetriever(parameters.url));
    ScheduleCommand(std::move(command));
    return test_future.Get();
  }

  std::unique_ptr<InstallIsolatedAppCommand> CreateCommand(
      base::StringPiece url,
      std::unique_ptr<content::WebContents> web_contents,
      const IsolationData& isolation_data,
      std::unique_ptr<WebAppUrlLoader> url_loader,
      base::OnceCallback<void(base::expected<InstallIsolatedAppCommandSuccess,
                                             InstallIsolatedAppCommandError>)>
          callback) {
    const GURL application_url{url};
    DCHECK(application_url.is_valid());

    return std::make_unique<InstallIsolatedAppCommand>(
        application_url, isolation_data, std::move(web_contents),
        std::move(url_loader), *install_finalizer_, std::move(callback));
  }

  base::expected<InstallIsolatedAppCommandSuccess,
                 InstallIsolatedAppCommandError>
  ExecuteCommandWithManifest(base::StringPiece application_url,
                             const blink::mojom::ManifestPtr& manifest) {
    auto url_loader = std::make_unique<TestWebAppUrlLoader>();
    url_loader->SetNextLoadUrlResult(GURL{application_url},
                                     WebAppUrlLoader::Result::kUrlLoaded);

    std::unique_ptr<MockDataRetriever> fake_data_retriever =
        CreateDefaultDataRetriever(application_url);

    ON_CALL(*fake_data_retriever, CheckInstallabilityAndRetrieveManifest)
        .WillByDefault(ReturnManifest(
            manifest, CreateDefaultManifestURL(application_url)));

    return ExecuteCommand(
        {
            .url = std::string(application_url),
            .url_loader = std::move(url_loader),
        },
        std::move(fake_data_retriever));
  }

  TestingProfile* profile() const { return profile_.get(); }

  FakeInstallFinalizer& install_finalizer() {
    DCHECK(install_finalizer_ != nullptr);
    return *install_finalizer_;
  }

 private:
  // Task environment allow to |base::OnceCallback| work in unit test.
  //
  // See details in //docs/threading_and_tasks_testing.md.
  content::BrowserTaskEnvironment browser_task_environment_;

  raw_ptr<FakeInstallFinalizer> install_finalizer_ = nullptr;

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
          [](const InstallIsolatedAppCommandError& error) {
            return error.message;
          },
          message_matcher)),
      arg, result_listener);
}

MATCHER(IsInstallationError, "") {
  return ExplainMatchResult(IsUnexpectedValue(_), arg, result_listener);
}

TEST_F(InstallIsolatedAppCommandTest,
       ServiceWorkerIsNotRequiredForInstallation) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(GURL{"http://test-url-example.com"},
                                   WebAppUrlLoader::Result::kUrlLoaded);

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever("http://test-url-example.com");

  EXPECT_CALL(*fake_data_retriever,
              CheckInstallabilityAndRetrieveManifest(
                  _, /*bypass_service_worker_check=*/IsTrue(), _))
      .WillOnce(ReturnManifest(
          CreateDefaultManifest("http://test-url-example.com"),
          CreateDefaultManifestURL("http://test-url-example.com")));

  EXPECT_THAT(ExecuteCommand(
                  Parameters{
                      .url = "http://test-url-example.com",
                      .url_loader = std::move(url_loader),
                  },
                  std::move(fake_data_retriever)),
              IsInstallationOk());
}

TEST_F(InstallIsolatedAppCommandTest, CommandCanBeExecutedSuccesfully) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(GURL{"http://test-url-example.com"},
                                   WebAppUrlLoader::Result::kUrlLoaded);

  EXPECT_THAT(ExecuteCommand(Parameters{
                  .url = "http://test-url-example.com",
                  .url_loader = std::move(url_loader),
              }),
              IsInstallationOk());
}

TEST_F(InstallIsolatedAppCommandTest, PropagateErrorWhenURLLoaderFails) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(
      GURL{"http://test-url-example.com"},
      WebAppUrlLoader::Result::kFailedErrorPageLoaded);

  EXPECT_THAT(ExecuteCommand(Parameters{
                  .url = "http://test-url-example.com",
                  .url_loader = std::move(url_loader),
              }),
              IsInstallationError(HasSubstr("Error during URL loading: ")));
}

TEST_F(InstallIsolatedAppCommandTest,
       PropagateErrorWhenURLLoaderFailsWithDestroyedWebContentsError) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(
      GURL{"http://test-url-example.com"},
      WebAppUrlLoaderResult::kFailedWebContentsDestroyed);

  EXPECT_THAT(ExecuteCommand(Parameters{
                  .url = "http://test-url-example.com",
                  .url_loader = std::move(url_loader),
              }),
              IsInstallationError(HasSubstr(
                  "Error during URL loading: FailedWebContentsDestroyed")));
}

TEST_F(InstallIsolatedAppCommandTest,
       URLLoaderIsCalledWithURLgivenToTheInstallCommand) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(GURL{"http://another-test-url-example.com"},
                                   WebAppUrlLoader::Result::kUrlLoaded);

  EXPECT_THAT(ExecuteCommand(Parameters{
                  .url = "http://another-test-url-example.com",
                  .url_loader = std::move(url_loader),
              }),
              IsInstallationOk());
}

TEST_F(InstallIsolatedAppCommandTest, URLLoaderIgnoresQueryParameters) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(GURL{"http://test-url-example.com"},
                                   WebAppUrlLoader::Result::kUrlLoaded);

  absl::optional<WebAppUrlLoader::UrlComparison> last_url_comparison =
      absl::nullopt;
  url_loader->TrackLoadUrlCalls(base::BindLambdaForTesting(
      [&](const GURL& unused_url, content::WebContents* unused_web_contents,
          WebAppUrlLoader::UrlComparison url_comparison) {
        last_url_comparison = url_comparison;
      }));

  EXPECT_THAT(ExecuteCommand(Parameters{
                  .url = "http://test-url-example.com",
                  .url_loader = std::move(url_loader),
              }),
              IsInstallationOk());

  EXPECT_THAT(
      last_url_comparison,
      Optional(Eq(WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef)));
}

TEST_F(InstallIsolatedAppCommandTest,
       InstallationFailsWhenFinalizerReturnNotInstallableError) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(GURL{"http://test-url-example.com"},
                                   WebAppUrlLoader::Result::kUrlLoaded);

  install_finalizer().SetNextFinalizeInstallResult(
      GenerateAppIdFromUnhashed("http://testing-unused-app-id.com/"),
      webapps::InstallResultCode::kNotInstallable);

  EXPECT_THAT(ExecuteCommand(Parameters{
                  .url = "http://test-url-example.com",
                  .url_loader = std::move(url_loader),
              }),
              IsInstallationError(
                  HasSubstr("Error during finalization: kNotInstallable")));
}

TEST_F(InstallIsolatedAppCommandTest,
       InstallationFailsWhenFinalizerReturnInstallURLLoadTimeOut) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(GURL{"http://test-url-example.com"},
                                   WebAppUrlLoader::Result::kUrlLoaded);

  install_finalizer().SetNextFinalizeInstallResult(
      GenerateAppIdFromUnhashed("http://testing-unused-app-id.com/"),
      webapps::InstallResultCode::kInstallURLLoadTimeOut);

  EXPECT_THAT(ExecuteCommand(Parameters{
                  .url = "http://test-url-example.com",
                  .url_loader = std::move(url_loader),
              }),
              IsInstallationError(HasSubstr(
                  "Error during finalization: kInstallURLLoadTimeOut")));
}

TEST_F(InstallIsolatedAppCommandTest,
       InstallationSucceedesWhenFinalizerReturnSuccessNewInstall) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(GURL{"http://test-url-example.com"},
                                   WebAppUrlLoader::Result::kUrlLoaded);

  install_finalizer().SetNextFinalizeInstallResult(
      GenerateAppIdFromUnhashed("http://testing-unused-app-id.com/"),
      webapps::InstallResultCode::kSuccessNewInstall);

  EXPECT_THAT(ExecuteCommand(Parameters{
                  .url = "http://test-url-example.com",
                  .url_loader = std::move(url_loader),
              }),
              IsInstallationOk());
}

TEST_F(InstallIsolatedAppCommandTest,
       InstallationFinalizedWithIsolatedAppDevInstallInstallSource) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(GURL{"http://test-url-example.com"},
                                   WebAppUrlLoader::Result::kUrlLoaded);

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever("http://test-url-example.com");

  EXPECT_THAT(ExecuteCommand(
                  Parameters{
                      .url = "http://test-url-example.com",
                      .url_loader = std::move(url_loader),
                  },
                  std::move(fake_data_retriever)),
              IsInstallationOk());

  using FinalizeOptions = WebAppInstallFinalizer::FinalizeOptions;
  using InstallSource = webapps::WebappInstallSource;

  EXPECT_THAT(
      install_finalizer().finalize_options_list(),
      ElementsAre(AllOf(Field(&FinalizeOptions::install_surface,
                              Eq(InstallSource::ISOLATED_APP_DEV_INSTALL)),
                        Field(&FinalizeOptions::source,
                              Eq(WebAppManagement::Type::kCommandLine)))));
}

TEST_F(InstallIsolatedAppCommandTest,
       InstallationFailsWhenAppIsNotInstallable) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(GURL{"http://test-url-example.com"},
                                   WebAppUrlLoader::Result::kUrlLoaded);

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever("http://test-url-example.com");

  ON_CALL(*fake_data_retriever, CheckInstallabilityAndRetrieveManifest)
      .WillByDefault(
          ReturnManifest(blink::mojom::Manifest::New(),
                         GURL{"http://test-url-example.com/manifest.json"},
                         /*is_installable=*/false));

  EXPECT_THAT(ExecuteCommand(
                  {
                      .url = "http://test-url-example.com",
                      .url_loader = std::move(url_loader),
                  },
                  std::move(fake_data_retriever)),
              IsInstallationError(HasSubstr("App is not installable")));
}

TEST_F(InstallIsolatedAppCommandTest, CommandLocksOnAppIdAndWebContents) {
  base::test::TestFuture<base::expected<InstallIsolatedAppCommandSuccess,
                                        InstallIsolatedAppCommandError>>
      test_future;

  auto command = CreateCommand(
      "http://test-app-id.com/",
      content::WebContents::Create(
          content::WebContents::CreateParams(profile())),
      CreateDefaultIsolationData(), std::make_unique<TestWebAppUrlLoader>(),
      test_future.GetCallback());
  EXPECT_THAT(command->lock(),
              AllOf(Property(&Lock::type, Eq(Lock::Type::kApp)),
                    Property(&Lock::app_ids,
                             UnorderedElementsAre(GenerateAppIdFromUnhashed(
                                 "http://test-app-id.com/")))));
}

TEST_F(InstallIsolatedAppCommandTest,
       InstallationFailsWhenAppIsInstallableButManifestIsNull) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(GURL{"http://test-url-example.com"},
                                   WebAppUrlLoader::Result::kUrlLoaded);

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever("http://test-url-example.com");

  ON_CALL(*fake_data_retriever, CheckInstallabilityAndRetrieveManifest)
      .WillByDefault(ReturnManifest(
          /*manifest=*/nullptr,
          CreateDefaultManifestURL("http://test-url-example.com")));

  EXPECT_THAT(ExecuteCommand(
                  Parameters{
                      .url = "http://test-url-example.com",
                      .url_loader = std::move(url_loader),
                  },
                  std::move(fake_data_retriever)),
              IsInstallationError(HasSubstr("Manifest is null")));
}

TEST_F(InstallIsolatedAppCommandTest, IsolationDataSentToFinalizer) {
  std::string url("http://test-url-example.com/");

  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(GURL{url},
                                   WebAppUrlLoader::Result::kUrlLoaded);

  EXPECT_THAT(ExecuteCommand(Parameters{
                  .url = url,
                  .url_loader = std::move(url_loader),
                  .isolation_data = IsolationData{IsolationData::DevModeProxy{
                      .proxy_url = "http://some-testing-proxy-url.com/"}},
              }),
              IsInstallationOk());

  EXPECT_THAT(install_finalizer().finalize_options_list(),
              ElementsAre(Field(
                  &WebAppInstallFinalizer::FinalizeOptions::isolation_data,
                  Optional(Field(
                      "content", &IsolationData::content,
                      VariantWith<IsolationData::DevModeProxy>(Field(
                          "proxy_url", &IsolationData::DevModeProxy::proxy_url,
                          Eq("http://some-testing-proxy-url.com/"))))))));
}

using InstallIsolatedAppCommandManifestTest = InstallIsolatedAppCommandTest;

TEST_F(InstallIsolatedAppCommandManifestTest,
       InstallationFailsWhenManifestHasNoId) {
  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest("http://manifest-test-url.com");
  manifest->id = absl::nullopt;

  EXPECT_THAT(
      ExecuteCommandWithManifest("http://manifest-test-url.com",
                                 manifest.Clone()),

      IsInstallationError(HasSubstr(
          "Manifest `id` is not present. manifest_url: " +
          CreateDefaultManifestURL("http://manifest-test-url.com").spec())));

  EXPECT_THAT(install_finalizer().web_app_info(), IsNull());
}

TEST_F(InstallIsolatedAppCommandManifestTest,
       FailsWhenManifestIdHasInvalidUTF8Character) {
  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest("http://manifest-test-url.com");
  char16_t invalid_utf8_chars = {0xD801};
  manifest->id = std::u16string{invalid_utf8_chars};

  EXPECT_THAT(ExecuteCommandWithManifest("http://manifest-test-url.com",
                                         manifest.Clone()),
              IsInstallationError(HasSubstr(
                  "Failed to convert manifest `id` from UTF16 to UTF8")));
}

TEST_F(InstallIsolatedAppCommandManifestTest,
       PassesManifestIdToFinalizerWhenManifestIdIsEmpty) {
  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest("http://manifest-test-url.com");
  manifest->id = u"";

  EXPECT_THAT(ExecuteCommandWithManifest("http://manifest-test-url.com",
                                         manifest.Clone()),
              IsInstallationOk());

  EXPECT_THAT(install_finalizer().web_app_info(),
              Pointee(Field(&WebAppInstallInfo::manifest_id,
                            Optional(std::string{""}))));
}

TEST_F(InstallIsolatedAppCommandManifestTest, FailsWhenManifestIdIsNotEmpty) {
  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest("http://manifest-test-url.com");
  manifest->id = u"test-manifest-id";

  EXPECT_THAT(ExecuteCommandWithManifest("http://manifest-test-url.com",
                                         manifest.Clone()),
              IsInstallationError(HasSubstr(R"(Manifest `id` must be "/")")));
  EXPECT_THAT(install_finalizer().web_app_info(), IsNull());
}

TEST_F(InstallIsolatedAppCommandManifestTest,
       FailsWhenManifestScopeIsNotSlash) {
  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest("http://manifest-test-url.com");

  manifest->scope = GURL{"http://manifest-test-url.com"}.Resolve("/scope");

  EXPECT_THAT(
      ExecuteCommandWithManifest("http://manifest-test-url.com",
                                 manifest.Clone()),
      IsInstallationError(HasSubstr("Scope should resolve to the origin")));
  EXPECT_THAT(install_finalizer().web_app_info(), IsNull());
}

TEST_F(InstallIsolatedAppCommandManifestTest,
       PassesManifestScopeToFinalizerWhenManifestScopeIsSlash) {
  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest("http://manifest-test-url.com");
  manifest->scope = GURL{"http://manifest-test-url.com"}.Resolve("/");

  EXPECT_THAT(ExecuteCommandWithManifest("http://manifest-test-url.com",
                                         manifest.Clone()),
              IsInstallationOk());

  EXPECT_THAT(install_finalizer().web_app_info(),
              Pointee(Field(&WebAppInstallInfo::scope,
                            GURL{"http://manifest-test-url.com/"})));
}

TEST_F(InstallIsolatedAppCommandManifestTest, PassesManifestNameAsTitle) {
  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest("http://manifest-test-url.com");
  manifest->name = u"test application name";

  EXPECT_THAT(ExecuteCommandWithManifest("http://manifest-test-url.com",
                                         manifest.Clone()),
              IsInstallationOk());

  EXPECT_THAT(
      install_finalizer().web_app_info(),
      Pointee(Field(&WebAppInstallInfo::title, u"test application name")));
}

TEST_F(InstallIsolatedAppCommandManifestTest,
       UseShortNameAsTitleWhenNameIsNotPresent) {
  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest("http://manifest-test-url.com");
  manifest->name = absl::nullopt;
  manifest->short_name = u"test short name";

  EXPECT_THAT(ExecuteCommandWithManifest("http://manifest-test-url.com",
                                         manifest.Clone()),
              IsInstallationOk());

  EXPECT_THAT(install_finalizer().web_app_info(),
              Pointee(Field(&WebAppInstallInfo::title, u"test short name")));
}

TEST_F(InstallIsolatedAppCommandManifestTest,
       UseShortNameAsTitleWhenNameIsEmpty) {
  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest("http://manifest-test-url.com");
  manifest->name = u"";
  manifest->short_name = u"other test short name";

  EXPECT_THAT(ExecuteCommandWithManifest("http://manifest-test-url.com",
                                         manifest.Clone()),
              IsInstallationOk());

  EXPECT_THAT(
      install_finalizer().web_app_info(),
      Pointee(Field(&WebAppInstallInfo::title, u"other test short name")));
}

TEST_F(InstallIsolatedAppCommandManifestTest,
       TitleIsmptyWhenNameAndShortNameAreNotPresent) {
  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest("http://manifest-test-url.com");
  manifest->name = absl::nullopt;
  manifest->short_name = absl::nullopt;

  EXPECT_THAT(ExecuteCommandWithManifest("http://manifest-test-url.com",
                                         manifest.Clone()),
              IsInstallationOk());

  EXPECT_THAT(install_finalizer().web_app_info(),
              Pointee(Field(&WebAppInstallInfo::title, IsEmpty())));
}

class InstallIsolatedAppCommandManifestIconsTest
    : public InstallIsolatedAppCommandManifestTest {
 protected:
  static constexpr base::StringPiece kSomeTestApplicationUrl =
      "http://manifest-test-url.com";
  void SetUp() override { InstallIsolatedAppCommandManifestTest::SetUp(); }

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

blink::Manifest::ImageResource CreateImageResource(GURL image_src) {
  blink::Manifest::ImageResource image;
  image.type = u"image/png";
  image.sizes.push_back(gfx::Size{kImageSize, kImageSize});
  image.purpose = {
      blink::mojom::ManifestImageResource_Purpose::ANY,
  };
  image.src = image_src;
  return image;
}

TEST_F(InstallIsolatedAppCommandManifestIconsTest, ManifestIconIsDownloaded) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(GURL{kSomeTestApplicationUrl},
                                   WebAppUrlLoader::Result::kUrlLoaded);

  blink::mojom::ManifestPtr manifest = CreateManifest();

  manifest->icons = {
      CreateImageResource(GURL{"http://test-icon-url.com/icon.png"})};

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateFakeDataRetriever(manifest.Clone());

  ON_CALL(*fake_data_retriever, CheckInstallabilityAndRetrieveManifest)
      .WillByDefault(ReturnManifest(
          manifest, CreateDefaultManifestURL(kSomeTestApplicationUrl)));

  std::map<GURL, std::vector<SkBitmap>> icons = {{
      GURL{"http://test-icon-url.com/icon.png"},
      {CreateTestBitmap(SK_ColorRED)},
  }};

  using HttpStatusCode = int;
  std::map<GURL, HttpStatusCode> http_result = {
      {GURL{"http://test-icon-url.com/icon.png"}, net::HttpStatusCode::HTTP_OK},
  };

  EXPECT_CALL(
      *fake_data_retriever,
      GetIcons(_,
               UnorderedElementsAre(GURL{"http://test-icon-url.com/icon.png"}),
               /*skip_page_favicons=*/true, IsNotNullCallback()))
      .WillOnce(RunOnceCallback<3>(IconsDownloadedResult::kCompleted,
                                   std::move(icons), http_result));

  EXPECT_THAT(ExecuteCommand(
                  {
                      .url = std::string(kSomeTestApplicationUrl),
                      .url_loader = std::move(url_loader),
                  },
                  std::move(fake_data_retriever)),
              IsInstallationOk());

  EXPECT_THAT(
      install_finalizer().web_app_info(),
      Pointee(ResultOf(
          [](const WebAppInstallInfo& info) { return info.icon_bitmaps.any; },
          Contains(Pair(_, ResultOf(
                               [](const SkBitmap& bitmap) {
                                 return bitmap.getColor(0, 0);
                               },
                               SK_ColorRED))))));
}

TEST_F(InstallIsolatedAppCommandManifestIconsTest,
       InstallationFailsWhenIconDownloadingFails) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(GURL{kSomeTestApplicationUrl},
                                   WebAppUrlLoader::Result::kUrlLoaded);

  blink::mojom::ManifestPtr manifest = CreateManifest();

  manifest->icons = {
      CreateImageResource(GURL{"http://test-icon-url.com/icon.png"})};

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
                  {
                      .url = std::string(kSomeTestApplicationUrl),
                      .url_loader = std::move(url_loader),
                  },
                  std::move(fake_data_retriever)),
              IsInstallationError(HasSubstr(
                  "Error during icon downloading: AbortedDueToFailure")));
}

TEST_F(InstallIsolatedAppCommandTest, SetDevModeIsolationDataBeforeUrlLoading) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(GURL{"http://test-url-example.com"},
                                   WebAppUrlLoader::Result::kUrlLoaded);

  absl::optional<IsolationData> isolation_data = absl::nullopt;
  url_loader->TrackLoadUrlCalls(base::BindLambdaForTesting(
      [&](const GURL& unused_url, content::WebContents* web_contents,
          WebAppUrlLoader::UrlComparison unused_url_comparison) {
        isolation_data =
            IsolatedWebAppPendingInstallInfo::FromWebContents(*web_contents)
                .isolation_data();
      }));

  EXPECT_THAT(ExecuteCommand({
                  .url = "http://test-url-example.com",
                  .url_loader = std::move(url_loader),
                  .isolation_data = IsolationData{IsolationData::DevModeProxy{
                      .proxy_url = "http://some-testing-proxy-url.com/"}},
              }),
              IsInstallationOk());

  EXPECT_THAT(
      isolation_data,
      Optional(Field("content", &IsolationData::content,
                     VariantWith<IsolationData::DevModeProxy>(Field(
                         "proxy_url", &IsolationData::DevModeProxy::proxy_url,
                         Eq("http://some-testing-proxy-url.com/"))))));
}

TEST_F(InstallIsolatedAppCommandTest,
       SetInstalledBundleIsolationDataBeforeUrlLoading) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(GURL{"http://test-url-example.com"},
                                   WebAppUrlLoader::Result::kUrlLoaded);

  absl::optional<IsolationData> isolation_data = absl::nullopt;
  url_loader->TrackLoadUrlCalls(base::BindLambdaForTesting(
      [&](const GURL& unused_url, content::WebContents* web_contents,
          WebAppUrlLoader::UrlComparison unused_url_comparison) {
        isolation_data =
            IsolatedWebAppPendingInstallInfo::FromWebContents(*web_contents)
                .isolation_data();
      }));

  EXPECT_THAT(
      ExecuteCommand({
          .url = "http://test-url-example.com",
          .url_loader = std::move(url_loader),
          .isolation_data = IsolationData{IsolationData::InstalledBundle{
              .path = base::FilePath{FILE_PATH_LITERAL(
                  "/testing/path/to/a/bundle")}}},
      }),
      IsInstallationOk());

  EXPECT_THAT(
      isolation_data,
      Optional(Field("content", &IsolationData::content,
                     VariantWith<IsolationData::InstalledBundle>(Field(
                         "proxy_url", &IsolationData::InstalledBundle::path,
                         Eq(base::FilePath{FILE_PATH_LITERAL(
                             "/testing/path/to/a/bundle")}))))));
}

using InstallIsolatedAppCommandMetricsTest = InstallIsolatedAppCommandTest;

TEST_F(InstallIsolatedAppCommandMetricsTest,
       ReportSuccessWhenFinishedSuccessfully) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(GURL{"http://test-url-example.com"},
                                   WebAppUrlLoader::Result::kUrlLoaded);

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommand(Parameters{
                  .url = "http://test-url-example.com",
                  .url_loader = std::move(url_loader),
              }),
              IsInstallationOk());

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Install.Result"),
              BucketsAre(base::Bucket(true, 1)));
}

TEST_F(InstallIsolatedAppCommandMetricsTest, ReportErrorWhenUrlLoaderFails) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(
      GURL{"http://test-url-example.com"},
      WebAppUrlLoader::Result::kFailedErrorPageLoaded);

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommand(Parameters{
                  .url = "http://test-url-example.com",
                  .url_loader = std::move(url_loader),
              }),
              IsInstallationError());

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Install.Result"),
              BucketsAre(base::Bucket(false, 1)));
}

TEST_F(InstallIsolatedAppCommandMetricsTest,
       ReportFailureWhenAppIsNotInstallable) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(GURL{"http://test-url-example.com"},
                                   WebAppUrlLoader::Result::kUrlLoaded);

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever("http://test-url-example.com");

  ON_CALL(*fake_data_retriever, CheckInstallabilityAndRetrieveManifest)
      .WillByDefault(
          ReturnManifest(blink::mojom::Manifest::New(),
                         GURL{"http://test-url-example.com/manifest.json"},
                         /*is_installable=*/false));

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommand(
                  {
                      .url = "http://test-url-example.com",
                      .url_loader = std::move(url_loader),
                  },
                  std::move(fake_data_retriever)),
              IsInstallationError());

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Install.Result"),
              BucketsAre(base::Bucket(false, 1)));
}

TEST_F(InstallIsolatedAppCommandMetricsTest, ReportFailureWhenManifestIsNull) {
  auto url_loader = std::make_unique<TestWebAppUrlLoader>();
  url_loader->SetNextLoadUrlResult(GURL{"http://test-url-example.com"},
                                   WebAppUrlLoader::Result::kUrlLoaded);

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever("http://test-url-example.com");

  ON_CALL(*fake_data_retriever, CheckInstallabilityAndRetrieveManifest)
      .WillByDefault(ReturnManifest(
          /*manifest=*/nullptr,
          CreateDefaultManifestURL("http://test-url-example.com"),
          /*is_installable=*/false));

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommand(
                  {
                      .url = "http://test-url-example.com",
                      .url_loader = std::move(url_loader),
                  },
                  std::move(fake_data_retriever)),
              IsInstallationError());

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Install.Result"),
              BucketsAre(base::Bucket(false, 1)));
}

TEST_F(InstallIsolatedAppCommandMetricsTest,
       ReportFailureWhenManifestIdIsNotEmpty) {
  blink::mojom::ManifestPtr manifest =
      CreateDefaultManifest("http://manifest-test-url.com");
  manifest->id = u"test manifest id";

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommandWithManifest("http://manifest-test-url.com",
                                         manifest.Clone()),
              IsInstallationError());
  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Install.Result"),
              BucketsAre(base::Bucket(false, 1)));
}

}  // namespace
}  // namespace web_app
