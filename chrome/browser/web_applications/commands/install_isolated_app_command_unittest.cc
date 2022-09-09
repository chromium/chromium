// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_isolated_app_command.h"

#include <map>
#include <memory>
#include <sstream>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
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
using ::testing::WithArg;

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

    auto url_loader = std::make_unique<TestWebAppUrlLoader>();
    url_loader_ = url_loader.get();
    provider->GetCommandManager().SetUrlLoaderForTesting(std::move(url_loader));
    auto install_finalizer = std::make_unique<FakeInstallFinalizer>();
    install_finalizer_ = install_finalizer.get();
    provider->SetInstallFinalizer(std::move(install_finalizer));

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void SetPrepareForLoadResultLoaded() {
    url_loader_->SetPrepareForLoadResultLoaded();
  }

  std::unique_ptr<InstallIsolatedAppCommand> CreateCommand(
      base::StringPiece url,
      base::OnceCallback<void(base::expected<InstallIsolatedAppCommandSuccess,
                                             InstallIsolatedAppCommandError>)>
          callback) {
    return std::make_unique<InstallIsolatedAppCommand>(
        GURL(url), *url_loader_, *install_finalizer_, std::move(callback));
  }

  void ScheduleCommand(std::unique_ptr<WebAppCommand> command) {
    WebAppProvider::GetForTest(profile())->command_manager().ScheduleCommand(
        std::move(command));
  }

  void ExpectLoadedForURL(base::StringPiece url) {
    DCHECK(GURL{url}.is_valid());
    url_loader_->SetNextLoadUrlResult(GURL{url},
                                      WebAppUrlLoader::Result::kUrlLoaded);
  }

  void ExpectFailureForURL(base::StringPiece url) {
    DCHECK(GURL{url}.is_valid());
    url_loader_->SetNextLoadUrlResult(
        GURL{url}, WebAppUrlLoader::Result::kFailedErrorPageLoaded);
  }

  void ExpectFailureForURL(base::StringPiece url,
                           WebAppUrlLoader::Result result) {
    DCHECK(GURL{url}.is_valid());
    url_loader_->SetNextLoadUrlResult(GURL{url}, result);
  }

  WebAppUrlLoader::UrlComparison last_call_url_comparison() const {
    absl::optional<TestWebAppUrlLoader::LoadUrlCall> last_url_load_call =
        url_loader_->last_load_url_call();
    DCHECK(last_url_load_call.has_value());
    return last_url_load_call->url_comparison;
  }

  base::expected<InstallIsolatedAppCommandSuccess,
                 InstallIsolatedAppCommandError>
  ExecuteCommand(
      base::StringPiece url,
      std::unique_ptr<WebAppDataRetriever> data_retriever = nullptr) {
    base::test::TestFuture<base::expected<InstallIsolatedAppCommandSuccess,
                                          InstallIsolatedAppCommandError>>
        test_future;
    auto command = CreateCommand(url, test_future.GetCallback());
    command->SetDataRetrieverForTesting(data_retriever != nullptr
                                            ? std::move(data_retriever)
                                            : CreateDefaultDataRetriever(url));
    ScheduleCommand(std::move(command));
    return test_future.Get();
  }

  base::expected<InstallIsolatedAppCommandSuccess,
                 InstallIsolatedAppCommandError>
  ExecuteCommandWithManifest(base::StringPiece application_url,
                             const blink::mojom::ManifestPtr& manifest) {
    SetPrepareForLoadResultLoaded();

    ExpectLoadedForURL(application_url);

    std::unique_ptr<MockDataRetriever> fake_data_retriever =
        CreateDefaultDataRetriever(application_url);

    ON_CALL(*fake_data_retriever, CheckInstallabilityAndRetrieveManifest)
        .WillByDefault(ReturnManifest(
            manifest, CreateDefaultManifestURL(application_url)));

    return ExecuteCommand(application_url, std::move(fake_data_retriever));
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

  raw_ptr<TestWebAppUrlLoader> url_loader_ = nullptr;
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
  SetPrepareForLoadResultLoaded();

  ExpectLoadedForURL("http://test-url-example.com");

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever("http://test-url-example.com");

  EXPECT_CALL(*fake_data_retriever,
              CheckInstallabilityAndRetrieveManifest(
                  _, /*bypass_service_worker_check=*/IsTrue(), _))
      .WillOnce(ReturnManifest(
          CreateDefaultManifest("http://test-url-example.com"),
          CreateDefaultManifestURL("http://test-url-example.com")));

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com",
                             std::move(fake_data_retriever)),
              IsInstallationOk());
}

TEST_F(InstallIsolatedAppCommandTest, CommandCanBeExecutedSuccesfully) {
  SetPrepareForLoadResultLoaded();

  ExpectLoadedForURL("http://test-url-example.com");

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com"),
              IsInstallationOk());
}

TEST_F(InstallIsolatedAppCommandTest, PropagateErrorWhenURLLoaderFails) {
  SetPrepareForLoadResultLoaded();

  ExpectFailureForURL("http://test-url-example.com");

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com"),
              IsInstallationError(HasSubstr("Error during URL loading: ")));
}

TEST_F(InstallIsolatedAppCommandTest,
       PropagateErrorWhenURLLoaderFailsWithDestroyedWebContentsError) {
  SetPrepareForLoadResultLoaded();

  ExpectFailureForURL("http://test-url-example.com",
                      WebAppUrlLoaderResult::kFailedWebContentsDestroyed);

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com"),
              IsInstallationError(HasSubstr(
                  "Error during URL loading: FailedWebContentsDestroyed")));
}

TEST_F(InstallIsolatedAppCommandTest,
       URLLoaderIsCalledWithURLgivenToTheInstallCommand) {
  SetPrepareForLoadResultLoaded();

  ExpectLoadedForURL("http://another-test-url-example.com");

  EXPECT_THAT(ExecuteCommand("http://another-test-url-example.com"),
              IsInstallationOk());
}

// It is impossible to pass invalid url to |GenerateAppId| since it DCHECKs.
// TODO(kuragin): Replace constructor with factory function to make the
// validation testable.
TEST_F(InstallIsolatedAppCommandTest, DISABLED_ReportsErrorWhenURLIsInvalid) {
  SetPrepareForLoadResultLoaded();

  EXPECT_THAT(ExecuteCommand("some definetely invalid url"),
              IsInstallationError(HasSubstr("Invalid application URL")));
}

TEST_F(InstallIsolatedAppCommandTest, URLLoaderIgnoresQueryParameters) {
  SetPrepareForLoadResultLoaded();

  ExpectLoadedForURL("http://test-url-example.com");

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com"),
              IsInstallationOk());

  EXPECT_THAT(last_call_url_comparison(),
              Eq(WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef));
}

TEST_F(InstallIsolatedAppCommandTest,
       InstallationFailsWhenFinalizerReturnNotInstallableError) {
  SetPrepareForLoadResultLoaded();

  ExpectLoadedForURL("http://test-url-example.com");

  install_finalizer().SetNextFinalizeInstallResult(
      GenerateAppIdFromUnhashed("http://testing-unused-app-id.com/"),
      webapps::InstallResultCode::kNotInstallable);

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com"),
              IsInstallationError(
                  HasSubstr("Error during finalization: kNotInstallable")));
}

TEST_F(InstallIsolatedAppCommandTest,
       InstallationFailsWhenFinalizerReturnInstallURLLoadTimeOut) {
  SetPrepareForLoadResultLoaded();

  ExpectLoadedForURL("http://test-url-example.com");

  install_finalizer().SetNextFinalizeInstallResult(
      GenerateAppIdFromUnhashed("http://testing-unused-app-id.com/"),
      webapps::InstallResultCode::kInstallURLLoadTimeOut);

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com"),
              IsInstallationError(HasSubstr(
                  "Error during finalization: kInstallURLLoadTimeOut")));
}

TEST_F(InstallIsolatedAppCommandTest,
       InstallationSucceedesWhenFinalizerReturnSuccessNewInstall) {
  SetPrepareForLoadResultLoaded();

  ExpectLoadedForURL("http://test-url-example.com");

  install_finalizer().SetNextFinalizeInstallResult(
      GenerateAppIdFromUnhashed("http://testing-unused-app-id.com/"),
      webapps::InstallResultCode::kSuccessNewInstall);

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com"),
              IsInstallationOk());
}

TEST_F(InstallIsolatedAppCommandTest,
       InstallationFinalizedWithIsolatedAppDevInstallInstallSource) {
  SetPrepareForLoadResultLoaded();

  ExpectLoadedForURL("http://test-url-example.com");

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever("http://test-url-example.com");

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com",
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
  SetPrepareForLoadResultLoaded();

  ExpectLoadedForURL("http://test-url-example.com");

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever("http://test-url-example.com");

  ON_CALL(*fake_data_retriever, CheckInstallabilityAndRetrieveManifest)
      .WillByDefault(
          ReturnManifest(blink::mojom::Manifest::New(),
                         GURL{"http://test-url-example.com/manifest.json"},
                         /*is_installable=*/false));

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com",
                             std::move(fake_data_retriever)),
              IsInstallationError(HasSubstr("App is not installable")));
}

TEST_F(InstallIsolatedAppCommandTest, CommandLocksOnAppIdAndWebContents) {
  base::test::TestFuture<base::expected<InstallIsolatedAppCommandSuccess,
                                        InstallIsolatedAppCommandError>>
      test_future;
  auto command =
      CreateCommand("http://test-app-id.com/", test_future.GetCallback());
  EXPECT_THAT(command->lock(),
              AllOf(Property(&Lock::type, Eq(Lock::Type::kAppAndWebContents)),
                    Property(&Lock::app_ids,
                             UnorderedElementsAre(GenerateAppIdFromUnhashed(
                                 "http://test-app-id.com/")))));
}

TEST_F(InstallIsolatedAppCommandTest,
       InstallationFailsWhenAppIsInstallableButManifestIsNull) {
  SetPrepareForLoadResultLoaded();

  ExpectLoadedForURL("http://test-url-example.com");

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever("http://test-url-example.com");

  ON_CALL(*fake_data_retriever, CheckInstallabilityAndRetrieveManifest)
      .WillByDefault(ReturnManifest(
          /*manifest=*/nullptr,
          CreateDefaultManifestURL("http://test-url-example.com")));

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com",
                             std::move(fake_data_retriever)),
              IsInstallationError(HasSubstr("Manifest is null")));
}

TEST_F(InstallIsolatedAppCommandTest, IsolationDataSentToFinalizer) {
  SetPrepareForLoadResultLoaded();

  std::string url("http://test-url-example.com/");
  ExpectLoadedForURL(url);

  EXPECT_THAT(ExecuteCommand(url), IsInstallationOk());

  EXPECT_THAT(
      install_finalizer().finalize_options_list(),
      ElementsAre(Field(
          &WebAppInstallFinalizer::FinalizeOptions::isolation_data,
          Eq(IsolationData(IsolationData::DevModeProxy{.proxy_url = url})))));
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
  void SetUp() override {
    InstallIsolatedAppCommandManifestTest::SetUp();

    SetPrepareForLoadResultLoaded();
    ExpectLoadedForURL(kSomeTestApplicationUrl);
  }

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

  EXPECT_THAT(
      ExecuteCommand(kSomeTestApplicationUrl, std::move(fake_data_retriever)),
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

  EXPECT_THAT(
      ExecuteCommand(kSomeTestApplicationUrl, std::move(fake_data_retriever)),
      IsInstallationError(
          HasSubstr("Error during icon downloading: AbortedDueToFailure")));
}

using InstallIsolatedAppCommandMetricsTest = InstallIsolatedAppCommandTest;

TEST_F(InstallIsolatedAppCommandMetricsTest,
       ReportSuccessWhenFinishedSuccessfully) {
  SetPrepareForLoadResultLoaded();

  ExpectLoadedForURL("http://test-url-example.com");

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com"),
              IsInstallationOk());

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Install.Result"),
              BucketsAre(base::Bucket(true, 1)));
}

TEST_F(InstallIsolatedAppCommandMetricsTest, ReportErrorWhenUrlLoaderFails) {
  SetPrepareForLoadResultLoaded();

  ExpectFailureForURL("http://test-url-example.com");

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com"),
              IsInstallationError());

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Install.Result"),
              BucketsAre(base::Bucket(false, 1)));
}

TEST_F(InstallIsolatedAppCommandMetricsTest,
       ReportFailureWhenAppIsNotInstallable) {
  SetPrepareForLoadResultLoaded();

  ExpectLoadedForURL("http://test-url-example.com");

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever("http://test-url-example.com");

  ON_CALL(*fake_data_retriever, CheckInstallabilityAndRetrieveManifest)
      .WillByDefault(
          ReturnManifest(blink::mojom::Manifest::New(),
                         GURL{"http://test-url-example.com/manifest.json"},
                         /*is_installable=*/false));

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com",
                             std::move(fake_data_retriever)),
              IsInstallationError());

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Install.Result"),
              BucketsAre(base::Bucket(false, 1)));
}

TEST_F(InstallIsolatedAppCommandMetricsTest, ReportFailureWhenManifestIsNull) {
  SetPrepareForLoadResultLoaded();

  ExpectLoadedForURL("http://test-url-example.com");

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever("http://test-url-example.com");

  ON_CALL(*fake_data_retriever, CheckInstallabilityAndRetrieveManifest)
      .WillByDefault(ReturnManifest(
          /*manifest=*/nullptr,
          CreateDefaultManifestURL("http://test-url-example.com"),
          /*is_installable=*/false));

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com",
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
