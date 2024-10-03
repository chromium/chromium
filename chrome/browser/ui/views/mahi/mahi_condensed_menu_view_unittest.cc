// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/mahi/mahi_condensed_menu_view.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/mahi/test/fake_mahi_web_contents_manager.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_constants.h"
#include "chromeos/components/mahi/public/cpp/mahi_browser_util.h"
#include "chromeos/components/mahi/public/cpp/mahi_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/test_event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace chromeos::mahi {
namespace {

using ::testing::Eq;

class MockMahiWebContentsManager : public ::mahi::FakeMahiWebContentsManager {
 public:
  MockMahiWebContentsManager() = default;
  ~MockMahiWebContentsManager() override = default;

  // ::mahi::FakeMahiWebContentsManager:
  MOCK_METHOD(void,
              OnContextMenuClicked,
              (int64_t display_id,
               ::chromeos::mahi::ButtonType button_type,
               const std::u16string& question,
               const gfx::Rect& mahi_menu_bounds),
              (override));
};

using MahiCondensedMenuViewTest = views::ViewsTestBase;

TEST_F(MahiCondensedMenuViewTest, NotifiesWebContentsManagerOnClick) {
  MockMahiWebContentsManager mock_mahi_web_contents_manager;
  chromeos::ScopedMahiWebContentsManagerOverride
      scoped_mahi_web_contents_manager(&mock_mahi_web_contents_manager);
  std::unique_ptr<views::Widget> menu_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  auto* condensed_menu_view =
      menu_widget->SetContentsView(std::make_unique<MahiCondensedMenuView>());
  menu_widget->Show();

  base::HistogramTester histogram;
  histogram.ExpectBucketCount(kMahiContextMenuButtonClickHistogram,
                              MahiMenuButton::kCondensedMenuButton, 0);

  // TODO(b/324647147): Add separate button type for condensed menu.
  EXPECT_CALL(
      mock_mahi_web_contents_manager,
      OnContextMenuClicked(
          Eq(display::Screen::GetScreen()
                 ->GetDisplayNearestWindow(menu_widget->GetNativeWindow())
                 .id()),
          Eq(::chromeos::mahi::ButtonType::kSummary), /*question=*/Eq(u""),
          /*mahi_menu_bounds=*/Eq(condensed_menu_view->GetBoundsInScreen())))
      .Times(1);

  ui::test::EventGenerator event_generator(GetContext(),
                                           menu_widget->GetNativeWindow());
  event_generator.MoveMouseTo(
      condensed_menu_view->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();

  histogram.ExpectBucketCount(kMahiContextMenuButtonClickHistogram,
                              MahiMenuButton::kCondensedMenuButton, 1);
}

}  // namespace
}  // namespace chromeos::mahi
