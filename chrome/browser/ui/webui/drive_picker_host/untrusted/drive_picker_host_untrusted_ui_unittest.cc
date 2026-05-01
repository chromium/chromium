// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/drive_picker_host/untrusted/drive_picker_host_untrusted_ui.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/drive_picker_host/drive_picker_result_handler.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockPage : public drive_picker_host_untrusted::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<drive_picker_host_untrusted::mojom::Page>
  BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(
      void,
      ShowDrivePicker,
      (mojo::PendingRemote<drive_picker_host::mojom::DrivePickerResultHandler>),
      (override));

 private:
  mojo::Receiver<drive_picker_host_untrusted::mojom::Page> receiver_{this};
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

class DrivePickerUntrustedHostUITest : public testing::Test {
 public:
  DrivePickerUntrustedHostUITest() = default;
  ~DrivePickerUntrustedHostUITest() override = default;

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

TEST_F(DrivePickerUntrustedHostUITest, IsWebUIEnabled_FeatureEnabled) {
  DrivePickerUntrustedHostUIConfig config;
  feature_list_.InitAndEnableFeature(
      omnibox::kComposeboxDriveContextMenuOption);
  EXPECT_TRUE(config.IsWebUIEnabled(profile()));
}

TEST_F(DrivePickerUntrustedHostUITest, IsWebUIEnabled_FeatureDisabled) {
  DrivePickerUntrustedHostUIConfig config;
  feature_list_.InitAndDisableFeature(
      omnibox::kComposeboxDriveContextMenuOption);
  EXPECT_FALSE(config.IsWebUIEnabled(profile()));
}

TEST_F(DrivePickerUntrustedHostUITest, ShowDrivePickerForwardsToPage) {
  content::TestWebUI test_web_ui;
  test_web_ui.set_web_contents(web_contents());
  DrivePickerUntrustedHostUI controller(&test_web_ui);

  MockPage mock_page;
  mojo::Remote<
      drive_picker_host_untrusted::mojom::DrivePickerUntrustedHostHandler>
      untrusted_host;
  controller.BindInterface(untrusted_host.BindNewPipeAndPassReceiver());

  untrusted_host->BindPage(mock_page.BindAndGetRemote());
  base::RunLoop().RunUntilIdle();

  MockResultHandler result_handler;
  EXPECT_CALL(mock_page, ShowDrivePicker(testing::_));
  controller.ShowDrivePicker(result_handler.BindAndGetRemote());

  // Need to flush mojo calls.
  base::RunLoop().RunUntilIdle();
}

TEST_F(DrivePickerUntrustedHostUITest, ShowDrivePickerQueuesUntilPageBound) {
  content::TestWebUI test_web_ui;
  test_web_ui.set_web_contents(web_contents());
  DrivePickerUntrustedHostUI controller(&test_web_ui);

  MockResultHandler result_handler;
  // Trigger before page is bound.
  controller.ShowDrivePicker(result_handler.BindAndGetRemote());

  MockPage mock_page;
  mojo::Remote<
      drive_picker_host_untrusted::mojom::DrivePickerUntrustedHostHandler>
      untrusted_host;
  controller.BindInterface(untrusted_host.BindNewPipeAndPassReceiver());

  EXPECT_CALL(mock_page, ShowDrivePicker(testing::_));
  untrusted_host->BindPage(mock_page.BindAndGetRemote());

  base::RunLoop().RunUntilIdle();
}

TEST_F(DrivePickerUntrustedHostUITest, ShowDrivePickerQueuesOnDisconnect) {
  content::TestWebUI test_web_ui;
  test_web_ui.set_web_contents(web_contents());
  DrivePickerUntrustedHostUI controller(&test_web_ui);

  {
    MockPage mock_page;
    controller.BindPage(mock_page.BindAndGetRemote());
    EXPECT_TRUE(controller.page_.is_connected());
  }

  // After MockPage is destroyed, it should be disconnected.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(controller.page_.is_connected());

  MockResultHandler result_handler;
  // This should now be queued because it's not connected.
  controller.ShowDrivePicker(result_handler.BindAndGetRemote());
  EXPECT_TRUE(controller.pending_request_.is_valid());

  MockPage mock_page2;
  EXPECT_CALL(mock_page2, ShowDrivePicker(testing::_));
  controller.BindPage(mock_page2.BindAndGetRemote());

  base::RunLoop().RunUntilIdle();
}
