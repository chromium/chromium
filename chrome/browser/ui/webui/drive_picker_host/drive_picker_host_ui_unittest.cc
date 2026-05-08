// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/drive_picker_host/drive_picker_host_ui.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/webui/drive_picker_host/untrusted/drive_picker_host_untrusted_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockDrivePickerBridge
    : public drive_picker_host_untrusted::mojom::DrivePickerBridge {
 public:
  MockDrivePickerBridge() = default;
  ~MockDrivePickerBridge() override = default;

  mojo::PendingRemote<drive_picker_host_untrusted::mojom::DrivePickerBridge>
  BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(
      void,
      ShowDrivePicker,
      (mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>),
      (override));

 private:
  mojo::Receiver<drive_picker_host_untrusted::mojom::DrivePickerBridge>
      receiver_{this};
};

class MockResultHandler
    : public drive_picker_host::mojom::DrivePickerResultHandler {
 public:
  MockResultHandler() = default;
  ~MockResultHandler() override = default;

  mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>
  BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<drive_picker_host::mojom::DrivePickerResultHandler> receiver_{
      this};
};

}  // namespace

class DrivePickerHostUITest : public testing::Test {
 public:
  DrivePickerHostUITest() = default;
  ~DrivePickerHostUITest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
  }

  TestingProfile* profile() { return profile_.get(); }
  content::WebContents* web_contents() { return web_contents_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(DrivePickerHostUITest, IsWebUIEnabled_FeatureEnabled) {
  DrivePickerHostUIConfig config;
  feature_list_.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOption);
  EXPECT_TRUE(config.IsWebUIEnabled(profile()));
}

TEST_F(DrivePickerHostUITest, IsWebUIEnabled_FeatureDisabled) {
  DrivePickerHostUIConfig config;
  feature_list_.InitAndDisableFeature(
      omnibox::kComposeboxDriveContextMenuOption);
  EXPECT_FALSE(config.IsWebUIEnabled(profile()));
}

TEST_F(DrivePickerHostUITest, TriggerDrivePickerHostForwardsToUntrusted) {
  feature_list_.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOption);

  content::TestWebUI test_web_ui;
  test_web_ui.set_web_contents(web_contents());
  DrivePickerHostUI controller(&test_web_ui);

  MockDrivePickerBridge mock_bridge;
  controller.SetBridge(mock_bridge.BindAndGetRemote());

  MockResultHandler result_handler;
  EXPECT_CALL(mock_bridge, ShowDrivePicker(testing::_));
  controller.TriggerDrivePickerHost(result_handler.BindAndGetRemote());

  base::RunLoop().RunUntilIdle();
}

TEST_F(DrivePickerHostUITest, TriggerDrivePickerHostQueuesUntilBridgeBound) {
  feature_list_.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOption);

  content::TestWebUI test_web_ui;
  test_web_ui.set_web_contents(web_contents());
  DrivePickerHostUI controller(&test_web_ui);

  MockResultHandler result_handler;
  // Trigger before bridge is set.
  controller.TriggerDrivePickerHost(result_handler.BindAndGetRemote());

  MockDrivePickerBridge mock_bridge;
  // Setting bridge should flush the pending request.
  EXPECT_CALL(mock_bridge, ShowDrivePicker(testing::_));
  controller.SetBridge(mock_bridge.BindAndGetRemote());

  base::RunLoop().RunUntilIdle();
}
