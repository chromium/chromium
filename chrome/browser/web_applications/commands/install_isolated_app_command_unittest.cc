// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/web_applications/commands/install_isolated_app_command.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/test/fake_data_retriever.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {
namespace {

using ::testing::Eq;
using ::testing::Not;

class InstallIsolatedAppCommandTest : public ::testing::Test {
 public:
  void SetUp() override {
    FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile());
    provider->SetDefaultFakeSubsystems();
    provider->SetRunSubsystemStartupTasks(true);

    auto url_loader = std::make_unique<TestWebAppUrlLoader>();
    url_loader_ = url_loader.get();
    provider->GetCommandManager().SetUrlLoaderForTesting(std::move(url_loader));

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void SetPrepareForLoadResultLoaded() {
    url_loader_->SetPrepareForLoadResultLoaded();
  }

  std::unique_ptr<InstallIsolatedAppCommand> CreateCommand(
      base::StringPiece url,
      base::OnceCallback<void(InstallIsolatedAppCommandResult)> callback) {
    return std::make_unique<InstallIsolatedAppCommand>(url, *url_loader_,
                                                       std::move(callback));
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

  static std::unique_ptr<FakeDataRetriever> CreateDefaultDataRetriever() {
    std::unique_ptr<FakeDataRetriever> fake_data_retriever =
        std::make_unique<FakeDataRetriever>();

    fake_data_retriever->SetManifest(
        /*manifest=*/CreateDefaultManifest(), /*is_installable=*/true,
        /*manifest_url=*/CreateDefaultManifestURL());
    return fake_data_retriever;
  }

  static blink::mojom::ManifestPtr CreateDefaultManifest() {
    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = GURL{"http://test.com/"},
    manifest->scope = GURL{"http://test.com/scope"},
    manifest->display = DisplayMode::kStandalone;
    manifest->short_name = u"Manifest Name";
    return manifest;
  }

  static GURL CreateDefaultManifestURL() {
    return GURL{"http://defaul-non-empty-url.com/manifest.json"};
  }

  TestingProfile* profile() const { return profile_.get(); }

 private:
  // Task environment allow to |base::OnceCallback| work in unit test.
  //
  // See details in //docs/threading_and_tasks_testing.md.
  content::BrowserTaskEnvironment browser_task_environment_;

  base::raw_ptr<TestWebAppUrlLoader> url_loader_ = nullptr;

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

TEST_F(InstallIsolatedAppCommandTest, StartCanBeStartedSuccesfully) {
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

TEST_F(InstallIsolatedAppCommandTest, SuccessWhenAppIsInstallable) {
  SetPrepareForLoadResultLoaded();

  ExpectLoadedForURL("http://test-url-example.com");

  std::unique_ptr<FakeDataRetriever> fake_data_retriever =
      std::make_unique<FakeDataRetriever>();
  fake_data_retriever->SetManifest(
      /*manifest=*/CreateDefaultManifest(), /*is_installable=*/true,
      /*manifest_url=*/CreateDefaultManifestURL());

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com",
                             std::move(fake_data_retriever)),
              IsInstallationOk());
}

TEST_F(InstallIsolatedAppCommandTest,
       InstallationFailsWhenAppIsNotInstallable) {
  SetPrepareForLoadResultLoaded();

  ExpectLoadedForURL("http://test-url-example.com");

  std::unique_ptr<FakeDataRetriever> fake_data_retriever =
      std::make_unique<FakeDataRetriever>();

  auto manifest = blink::mojom::Manifest::New();
  fake_data_retriever->SetManifest(
      std::move(manifest),
      /*is_installable=*/false,
      /*manifest_url=*/
      GURL{"http://test-url-example.com/manifest.json"});

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com",
                             std::move(fake_data_retriever)),
              Not(IsInstallationOk()));
}

TEST_F(InstallIsolatedAppCommandTest,
       InstallationFailsWhenAppIsInstallableButManifestIsNull) {
  SetPrepareForLoadResultLoaded();

  ExpectLoadedForURL("http://test-url-example.com");

  std::unique_ptr<FakeDataRetriever> fake_data_retriever =
      std::make_unique<FakeDataRetriever>();

  fake_data_retriever->SetManifest(
      /*manifest=*/nullptr,
      /*is_installable=*/true,
      /*manifest_url=*/CreateDefaultManifestURL());

  EXPECT_THAT(ExecuteCommand("http://test-url-example.com",
                             std::move(fake_data_retriever)),
              Not(IsInstallationOk()));
}

}  // namespace
}  // namespace web_app
