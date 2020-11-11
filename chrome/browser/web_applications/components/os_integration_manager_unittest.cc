// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/os_integration_manager.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/test_file_handler_manager.h"
#include "chrome/browser/web_applications/test/test_os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {
class OsIntegrationManagerTest : public WebAppTest {
 public:
  void SetUp() override {
    WebAppTest::SetUp();
    auto shortcut_manager = std::make_unique<TestShortcutManager>(profile());
    auto file_handler_manager =
        std::make_unique<TestFileHandlerManager>(profile());

    os_integration_manager_ = std::make_unique<OsIntegrationManager>(
        profile(), std::move(shortcut_manager),
        std::move(file_handler_manager));

    os_integration_manager_->SuppressOsManagersForTesting();
  }
  OsIntegrationManager* os_integration_manager() {
    return os_integration_manager_.get();
  }

 private:
  std::unique_ptr<WebAppUiManager> ui_manager_;
  std::unique_ptr<OsIntegrationManager> os_integration_manager_;
};

TEST_F(OsIntegrationManagerTest, InstallOsHooksCallbackCalled) {
  base::RunLoop run_loop;

  OsHooksResults install_results;
  InstallOsHooksCallback callback =
      base::BindLambdaForTesting([&](OsHooksResults results) {
        install_results = results;
        run_loop.Quit();
      });

  const AppId app_id = "test";
  InstallOsHooksOptions options;
  options.os_hooks = OsHooksResults{true};
  os_integration_manager()->InstallOsHooks(app_id, std::move(callback), nullptr,
                                           options);
  run_loop.Run();
  ASSERT_TRUE(install_results[0]);
}
}  // namespace web_app
