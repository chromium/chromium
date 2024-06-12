// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/desktop_capture/desktop_media_permission_pane_view.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::TaskEnvironment;

class DesktopMediaPermissionPaneViewTest : public ::testing::Test {
 public:
  ~DesktopMediaPermissionPaneViewTest() override = default;

 protected:
  TaskEnvironment task_environment_;
  base::MockRepeatingCallback<void()> open_screen_recording_settings_callback_;

  // Makes ChromeLayoutProvider available through the static
  // ChromeLayoutProvider::Get() accessor, which is used in
  // DesktopMediaPermissionPaneView.
  ChromeLayoutProvider layout_provider_;

  std::unique_ptr<DesktopMediaPermissionPaneView> permission_pane_view_ =
      std::make_unique<DesktopMediaPermissionPaneView>(
          DesktopMediaList::Type::kScreen,
          open_screen_recording_settings_callback_.Get());
};

TEST_F(DesktopMediaPermissionPaneViewTest, TestClick) {
  EXPECT_CALL(open_screen_recording_settings_callback_, Run).Times(1);
  permission_pane_view_->SimulateClickForTesting();
  task_environment_.RunUntilIdle();
}
