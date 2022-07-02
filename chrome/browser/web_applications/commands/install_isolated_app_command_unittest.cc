// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/web_applications/commands/install_isolated_app_command.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/test_web_app_url_loader.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"

namespace web_app {
namespace {

class InstallIsolatedAppCommandTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();
    FakeWebAppProvider* provider = FakeWebAppProvider::Get(profile());
    provider->SetDefaultFakeSubsystems();
    provider->SetRunSubsystemStartupTasks(true);

    auto url_loader = std::make_unique<TestWebAppUrlLoader>();
    url_loader_ = url_loader.get();
    provider->GetCommandManager().SetUrlLoaderForTesting(std::move(url_loader));

    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void AddPrepareForLoadResults(
      const std::vector<WebAppUrlLoader::Result>& results) {
    url_loader_->AddPrepareForLoadResults(results);
  }

 private:
  base::raw_ptr<TestWebAppUrlLoader> url_loader_;
};

TEST_F(InstallIsolatedAppCommandTest, StartCanBeStartedSuccesfully) {
  AddPrepareForLoadResults(std::vector<WebAppUrlLoader::Result>{
      WebAppUrlLoader::Result::kUrlLoaded,
  });

  auto subject = std::make_unique<InstallIsolatedAppCommand>(
      "some random application URL");
  WebAppProvider::GetForTest(profile())->command_manager().ScheduleCommand(
      std::move(subject));
}
}  // namespace
}  // namespace web_app
