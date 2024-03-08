// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/mahi/mahi_menu_view.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "chrome/browser/chromeos/mahi/mahi_browser_util.h"
#include "chrome/browser/chromeos/mahi/mahi_web_contents_manager.h"
#include "chrome/browser/chromeos/mahi/test/fake_mahi_web_contents_manager.h"
#include "chrome/browser/chromeos/mahi/test/scoped_mahi_web_contents_manager_for_testing.h"
#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace chromeos::mahi {

using MahiMenuViewTest = ChromeViewsTestBase;

namespace {

class MockMahiWebContentsManager : public ::mahi::FakeMahiWebContentsManager {
 public:
  MOCK_METHOD(void,
              OnContextMenuClicked,
              (int64_t display_id,
               ::mahi::ButtonType button_type,
               const std::u16string& question),
              (override));
};

}  // namespace

TEST_F(MahiMenuViewTest, Bounds) {
  const gfx::Rect anchor_view_bounds = gfx::Rect(50, 50, 25, 100);
  auto menu_widget = MahiMenuView::CreateWidget(anchor_view_bounds);

  // The bounds of the created widget should be similar to the value from the
  // utils function.
  EXPECT_EQ(editor_menu::GetEditorMenuBounds(
                anchor_view_bounds, menu_widget.get()->GetContentsView()),
            menu_widget->GetRestoredBounds());
}

TEST_F(MahiMenuViewTest, SummaryButtonClicked) {
  MockMahiWebContentsManager mock_mahi_web_contents_manager;
  auto scoped_mahi_web_contents_manager =
      std::make_unique<::mahi::ScopedMahiWebContentsManagerForTesting>(
          &mock_mahi_web_contents_manager);

  auto menu_widget = CreateTestWidget();
  auto* menu_view =
      menu_widget->SetContentsView(std::make_unique<MahiMenuView>());

  auto event_generator = std::make_unique<ui::test::EventGenerator>(
      views::GetRootWindow(menu_widget.get()));
  event_generator->MoveMouseTo(
      menu_view->summary_button_for_test()->GetBoundsInScreen().CenterPoint());

  // Make sure that clicking the summary button would trigger the function in
  // `MahiWebContentsManager` with the correct parameters.
  base::RunLoop run_loop;
  EXPECT_CALL(mock_mahi_web_contents_manager, OnContextMenuClicked)
      .WillOnce([&run_loop, &menu_widget](int64_t display_id,
                                          ::mahi::ButtonType button_type,
                                          const std::u16string& question) {
        EXPECT_EQ(display::Screen::GetScreen()
                      ->GetDisplayNearestWindow(menu_widget->GetNativeWindow())
                      .id(),
                  display_id);
        EXPECT_EQ(::mahi::ButtonType::kSummary, button_type);
        EXPECT_EQ(std::u16string(), question);
        run_loop.Quit();
      });

  event_generator->ClickLeftButton();
  run_loop.Run();
}

TEST_F(MahiMenuViewTest, OutlineButtonClicked) {
  MockMahiWebContentsManager mock_mahi_web_contents_manager;
  auto scoped_mahi_web_contents_manager =
      std::make_unique<::mahi::ScopedMahiWebContentsManagerForTesting>(
          &mock_mahi_web_contents_manager);

  auto menu_widget = CreateTestWidget();
  auto* menu_view =
      menu_widget->SetContentsView(std::make_unique<MahiMenuView>());

  auto event_generator = std::make_unique<ui::test::EventGenerator>(
      views::GetRootWindow(menu_widget.get()));
  event_generator->MoveMouseTo(
      menu_view->outline_button_for_test()->GetBoundsInScreen().CenterPoint());

  // Make sure that clicking the summary button would trigger the function in
  // `MahiWebContentsManager` with the correct parameters.
  base::RunLoop run_loop;
  EXPECT_CALL(mock_mahi_web_contents_manager, OnContextMenuClicked)
      .WillOnce([&run_loop, &menu_widget](int64_t display_id,
                                          ::mahi::ButtonType button_type,
                                          const std::u16string& question) {
        EXPECT_EQ(display::Screen::GetScreen()
                      ->GetDisplayNearestWindow(menu_widget->GetNativeWindow())
                      .id(),
                  display_id);
        EXPECT_EQ(::mahi::ButtonType::kOutline, button_type);
        EXPECT_EQ(std::u16string(), question);
        run_loop.Quit();
      });

  event_generator->ClickLeftButton();
  run_loop.Run();
}

}  // namespace chromeos::mahi
