// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/install_isolated_app_command.h"

#include <map>
#include <memory>
#include <sstream>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_piece_forward.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/test/fake_install_finalizer.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/mock_data_retriever.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "chrome/test/base/testing_profile.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-shared.h"
#include "url/gurl.h"

namespace web_app {
namespace {

using ::base::test::IsNotNullCallback;
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsFalse;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

blink::mojom::ManifestPtr CreateDefaultManifest() {
  auto manifest = blink::mojom::Manifest::New();
  manifest->start_url = GURL{"http://test.com/"},
  manifest->scope = GURL{"http://test.com/scope"},
  manifest->display = DisplayMode::kStandalone;
  manifest->short_name = u"Manifest Name";
  return manifest;
}

GURL CreateDefaultManifestURL() {
  return GURL{"http://defaul-non-empty-url.com/manifest.json"};
}

std::unique_ptr<MockDataRetriever> CreateDefaultDataRetriever() {
  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      std::make_unique<NiceMock<MockDataRetriever>>();

  EXPECT_CALL(*fake_data_retriever, GetWebAppInstallInfo).Times(0);

  ON_CALL(*fake_data_retriever,
          CheckInstallabilityAndRetrieveManifest(_, _, IsNotNullCallback()))
      .WillByDefault(RunOnceCallback<2>(
          /*manifest=*/CreateDefaultManifest(),
          /*manifest_url=*/CreateDefaultManifestURL(),
          /*valid_manifest_for_web_app=*/true,
          /*is_installable=*/true));

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
      base::OnceCallback<void(InstallIsolatedAppCommandResult)> callback) {
    return std::make_unique<InstallIsolatedAppCommand>(
        url, *url_loader_, *install_finalizer_, std::move(callback));
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

  WebAppUrlLoader::UrlComparison last_call_url_comparison() const {
    absl::optional<TestWebAppUrlLoader::LoadUrlCall> last_url_load_call =
        url_loader_->last_load_url_call();
    DCHECK(last_url_load_call.has_value());
    return last_url_load_call->url_comparison;
  }

  InstallIsolatedAppCommandResult ExecuteCommand(
      base::StringPiece url,
      std::unique_ptr<WebAppDataRetriever> data_retriever = nullptr) {
    base::test::TestFuture<InstallIsolatedAppCommandResult> test_future;
    auto command = CreateCommand(url, test_future.GetCallback());
    command->SetDataRetrieverForTesting(data_retriever != nullptr
                                            ? std::move(data_retriever)
                                            : CreateDefaultDataRetriever());
    ScheduleCommand(std::move(command));
    return test_future.Get();
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

MATCHER(IsInstallationOk, "") {
  return ExplainMatchResult(Eq(InstallIsolatedAppCommandResult::kOk), arg,
                            result_listener);
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
              Not(IsInstallationOk()));
}

TEST_F(InstallIsolatedAppCommandTest,
       URLLoaderIsCalledWithURLgivenToTheInstallCommand) {
  SetPrepareForLoadResultLoaded();

  ExpectLoadedForURL("http://another-test-url-example.com");

  EXPECT_THAT(ExecuteCommand("http://another-test-url-example.com"),
              IsInstallationOk());
}

TEST_F(InstallIsolatedAppCommandTest, ReportsErrorWhenURLIsInvalid) {
  SetPrepareForLoadResultLoaded();

  EXPECT_THAT(ExecuteCommand("some definetely invalid url"),
              Not(IsInstallationOk()));
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
       InstallationFinalizedWithManagementApiInstallSource) {
  SetPrepareForLoadResultLoaded();

  ExpectLoadedForURL("http://test-url-example.com");

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      CreateDefaultDataRetriever();

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com",
                             std::move(fake_data_retriever)),
              IsInstallationOk());

  EXPECT_THAT(install_finalizer().finalize_options_list(),
              ElementsAre(Field(
                  &WebAppInstallFinalizer::FinalizeOptions::install_surface,
                  Eq(webapps::WebappInstallSource::MANAGEMENT_API))));
}

TEST_F(InstallIsolatedAppCommandTest,
       InstallationFailsWhenAppIsNotInstallable) {
  SetPrepareForLoadResultLoaded();

  ExpectLoadedForURL("http://test-url-example.com");

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      std::make_unique<NiceMock<MockDataRetriever>>();

  ON_CALL(*fake_data_retriever,
          CheckInstallabilityAndRetrieveManifest(_, _, IsNotNullCallback()))
      .WillByDefault(RunOnceCallback<2>(
          /*manifest=*/blink::mojom::Manifest::New(),
          /*manifest_url=*/
          GURL{"http://test-url-example.com/manifest.json"},
          /*valid_manifest_for_web_app=*/true,
          /*is_installable=*/false));

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com",
                             std::move(fake_data_retriever)),
              Not(IsInstallationOk()));
}

TEST_F(InstallIsolatedAppCommandTest,
       InstallationFailsWhenAppIsInstallableButManifestIsNull) {
  SetPrepareForLoadResultLoaded();

  ExpectLoadedForURL("http://test-url-example.com");

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      std::make_unique<NiceMock<MockDataRetriever>>();

  ON_CALL(*fake_data_retriever,
          CheckInstallabilityAndRetrieveManifest(_, _, IsNotNullCallback()))
      .WillByDefault(RunOnceCallback<2>(
          /*manifest=*/nullptr,
          /*manifest_url=*/CreateDefaultManifestURL(),
          /*valid_manifest_for_web_app=*/true,
          /*is_installable=*/false));

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com",
                             std::move(fake_data_retriever)),
              Not(IsInstallationOk()));
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
              base::BucketsAre(base::Bucket(true, 1)));
}

TEST_F(InstallIsolatedAppCommandMetricsTest, ReportFailureWhenURLIsInvalid) {
  SetPrepareForLoadResultLoaded();

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommand("some definetely invalid url"),
              Not(IsInstallationOk()));

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Install.Result"),
              base::BucketsAre(base::Bucket(false, 1)));
}

TEST_F(InstallIsolatedAppCommandMetricsTest, ReportErrorWhenUrlLoaderFails) {
  SetPrepareForLoadResultLoaded();

  ExpectFailureForURL("http://test-url-example.com");

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com"),
              Not(IsInstallationOk()));

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Install.Result"),
              base::BucketsAre(base::Bucket(false, 1)));
}

TEST_F(InstallIsolatedAppCommandMetricsTest,
       ReportFailureWhenAppIsNotInstallable) {
  SetPrepareForLoadResultLoaded();

  ExpectLoadedForURL("http://test-url-example.com");

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      std::make_unique<NiceMock<MockDataRetriever>>();

  ON_CALL(*fake_data_retriever,
          CheckInstallabilityAndRetrieveManifest(_, _, IsNotNullCallback()))
      .WillByDefault(RunOnceCallback<2>(
          /*manifest=*/blink::mojom::Manifest::New(),
          /*manifest_url=*/
          GURL{"http://test-url-example.com/manifest.json"},
          /*valid_manifest_for_web_app=*/true,
          /*is_installable=*/false));

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com",
                             std::move(fake_data_retriever)),
              Not(IsInstallationOk()));

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Install.Result"),
              base::BucketsAre(base::Bucket(false, 1)));
}

TEST_F(InstallIsolatedAppCommandMetricsTest, ReportFailureWhenManifestIsNull) {
  SetPrepareForLoadResultLoaded();

  ExpectLoadedForURL("http://test-url-example.com");

  std::unique_ptr<MockDataRetriever> fake_data_retriever =
      std::make_unique<NiceMock<MockDataRetriever>>();

  ON_CALL(*fake_data_retriever,
          CheckInstallabilityAndRetrieveManifest(_, _, IsNotNullCallback()))
      .WillByDefault(RunOnceCallback<2>(
          /*manifest=*/nullptr,
          /*manifest_url=*/CreateDefaultManifestURL(),
          /*valid_manifest_for_web_app=*/true,
          /*is_installable=*/false));

  base::HistogramTester histogram_tester;

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com",
                             std::move(fake_data_retriever)),
              Not(IsInstallationOk()));

  EXPECT_THAT(histogram_tester.GetAllSamples("WebApp.Install.Result"),
              base::BucketsAre(base::Bucket(false, 1)));
}

}  // namespace
}  // namespace web_app
