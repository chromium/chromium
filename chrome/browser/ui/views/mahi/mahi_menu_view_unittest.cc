// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/mahi/mahi_menu_view.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/chromeos/mahi/test/fake_mahi_web_contents_manager.h"
#include "chrome/browser/ui/views/editor_menu/utils/utils.h"
#include "chrome/browser/ui/views/mahi/mahi_menu_constants.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "chromeos/components/mahi/public/cpp/mahi_browser_util.h"
#include "chromeos/components/mahi/public/cpp/mahi_web_contents_manager.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace chromeos::mahi {

using MahiMenuViewTest = ChromeViewsTestBase;

namespace {

using ::testing::Eq;

class MockMahiWebContentsManager : public ::mahi::FakeMahiWebContentsManager {
 public:
  MOCK_METHOD(void,
              OnContextMenuClicked,
              (int64_t display_id,
               ::chromeos::mahi::ButtonType button_type,
               const std::u16string& question,
               const gfx::Rect& mahi_menu_bounds),
              (override));
};

// A widget that always claims to be active, regardless of its real activation
// status.
class ActiveWidget : public views::Widget {
 public:
  ActiveWidget() = default;

  ActiveWidget(const ActiveWidget&) = delete;
  ActiveWidget& operator=(const ActiveWidget&) = delete;

  ~ActiveWidget() override = default;

  bool IsActive() const override { return true; }
};

// Helper function to simulate typing "TEST".
void TypeTestResponse(ui::test::EventGenerator* event_generator) {
  ui::KeyboardCode keycodes[] = {ui::VKEY_T, ui::VKEY_E, ui::VKEY_S,
                                 ui::VKEY_T};
  for (ui::KeyboardCode keycode : keycodes) {
    event_generator->PressAndReleaseKey(keycode, ui::EF_NONE);
  }
}

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

TEST_F(MahiMenuViewTest, SettingsButtonClicked) {
  base::HistogramTester histogram;
  MockMahiWebContentsManager mock_mahi_web_contents_manager;
  chromeos::ScopedMahiWebContentsManagerOverride
      scoped_mahi_web_contents_manager(&mock_mahi_web_contents_manager);

  std::unique_ptr<views::Widget> menu_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  auto* menu_view =
      menu_widget->SetContentsView(std::make_unique<MahiMenuView>());

  EXPECT_CALL(
      mock_mahi_web_contents_manager,
      OnContextMenuClicked(
          Eq(display::Screen::GetScreen()
                 ->GetDisplayNearestWindow(menu_widget->GetNativeWindow())
                 .id()),
          Eq(::chromeos::mahi::ButtonType::kSettings),
          /*question=*/Eq(u""), Eq(menu_view->GetBoundsInScreen())))
      .Times(1);

  ui::test::EventGenerator event_generator(
      views::GetRootWindow(menu_widget.get()));
  event_generator.MoveMouseTo(menu_view->GetViewByID(ViewID::kSettingsButton)
                                  ->GetBoundsInScreen()
                                  .CenterPoint());
  event_generator.ClickLeftButton();

  histogram.ExpectBucketCount(kMahiContextMenuButtonClickHistogram,
                              MahiMenuButton::kSettingsButton, 1);
}

TEST_F(MahiMenuViewTest, SummaryButtonClicked) {
  MockMahiWebContentsManager mock_mahi_web_contents_manager;
  auto scoped_mahi_web_contents_manager =
      std::make_unique<chromeos::ScopedMahiWebContentsManagerOverride>(
          &mock_mahi_web_contents_manager);

  auto menu_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  auto* menu_view =
      menu_widget->SetContentsView(std::make_unique<MahiMenuView>());

  auto event_generator = std::make_unique<ui::test::EventGenerator>(
      views::GetRootWindow(menu_widget.get()));
  event_generator->MoveMouseTo(menu_view->GetViewByID(ViewID::kSummaryButton)
                                   ->GetBoundsInScreen()
                                   .CenterPoint());

  base::HistogramTester histogram;
  histogram.ExpectBucketCount(kMahiContextMenuButtonClickHistogram,
                              MahiMenuButton::kSummaryButton, 0);

  // Make sure that clicking the summary button would trigger the function in
  // `MahiWebContentsManager` with the correct parameters.
  base::RunLoop run_loop;
  EXPECT_CALL(mock_mahi_web_contents_manager, OnContextMenuClicked)
      .WillOnce([&run_loop, &menu_widget](
                    int64_t display_id,
                    ::chromeos::mahi::ButtonType button_type,
                    const std::u16string& question,
                    gfx::Rect mahi_menu_bounds) {
        EXPECT_EQ(display::Screen::GetScreen()
                      ->GetDisplayNearestWindow(menu_widget->GetNativeWindow())
                      .id(),
                  display_id);
        EXPECT_EQ(::chromeos::mahi::ButtonType::kSummary, button_type);
        EXPECT_EQ(std::u16string(), question);
        run_loop.Quit();
      });

  event_generator->ClickLeftButton();
  run_loop.Run();

  histogram.ExpectBucketCount(kMahiContextMenuButtonClickHistogram,
                              MahiMenuButton::kSummaryButton, 1);
}

// TODO(b/330643995): Remove this test after outlines are shown by default.
TEST_F(MahiMenuViewTest, OutlineButtonHiddenByDefault) {
  auto menu_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  auto* menu_view =
      menu_widget->SetContentsView(std::make_unique<MahiMenuView>());

  EXPECT_FALSE(menu_view->GetViewByID(ViewID::kOutlineButton)->GetVisible());
}

TEST_F(MahiMenuViewTest, OutlineButtonClicked) {
  MockMahiWebContentsManager mock_mahi_web_contents_manager;
  auto scoped_mahi_web_contents_manager =
      std::make_unique<chromeos::ScopedMahiWebContentsManagerOverride>(
          &mock_mahi_web_contents_manager);

  auto menu_widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  auto* menu_view =
      menu_widget->SetContentsView(std::make_unique<MahiMenuView>());
  // TODO(b/330643995): After outlines are shown by default, remove this since
  // we won't need to explicitly show the outline button.
  menu_view->GetViewByID(ViewID::kOutlineButton)->SetVisible(true);
  views::test::RunScheduledLayout(menu_view);

  auto event_generator = std::make_unique<ui::test::EventGenerator>(
      views::GetRootWindow(menu_widget.get()));
  event_generator->MoveMouseTo(menu_view->GetViewByID(ViewID::kOutlineButton)
                                   ->GetBoundsInScreen()
                                   .CenterPoint());

  base::HistogramTester histogram;
  histogram.ExpectBucketCount(kMahiContextMenuButtonClickHistogram,
                              MahiMenuButton::kOutlineButton, 0);

  // Make sure that clicking the summary button would trigger the function in
  // `MahiWebContentsManager` with the correct parameters.
  base::RunLoop run_loop;
  EXPECT_CALL(mock_mahi_web_contents_manager, OnContextMenuClicked)
      .WillOnce([&run_loop, &menu_widget](
                    int64_t display_id,
                    ::chromeos::mahi::ButtonType button_type,
                    const std::u16string& question,
                    gfx::Rect mahi_menu_bounds) {
        EXPECT_EQ(display::Screen::GetScreen()
                      ->GetDisplayNearestWindow(menu_widget->GetNativeWindow())
                      .id(),
                  display_id);
        EXPECT_EQ(::chromeos::mahi::ButtonType::kOutline, button_type);
        EXPECT_EQ(std::u16string(), question);
        run_loop.Quit();
      });

  event_generator->ClickLeftButton();
  run_loop.Run();

  histogram.ExpectBucketCount(kMahiContextMenuButtonClickHistogram,
                              MahiMenuButton::kOutlineButton, 1);
}

TEST_F(MahiMenuViewTest, SubmitQuestionButtonEnabledAfterTextInput) {
  auto menu_widget = std::make_unique<ActiveWidget>();
  menu_widget->Init(CreateParamsForTestWidget());

  auto* menu_view =
      menu_widget->SetContentsView(std::make_unique<MahiMenuView>());

  auto event_generator = std::make_unique<ui::test::EventGenerator>(
      views::GetRootWindow(menu_widget.get()));

  auto* submit_question_button =
      menu_view->GetViewByID(ViewID::kSubmitQuestionButton);
  auto* textfield = menu_view->GetViewByID(ViewID::kTextfield);

  EXPECT_FALSE(submit_question_button->GetEnabled());

  event_generator->MoveMouseTo(textfield->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();

  TypeTestResponse(event_generator.get());

  EXPECT_TRUE(submit_question_button->GetEnabled());
}

TEST_F(MahiMenuViewTest, QuestionSubmitted) {
  MockMahiWebContentsManager mock_mahi_web_contents_manager;
  auto scoped_mahi_web_contents_manager =
      std::make_unique<chromeos::ScopedMahiWebContentsManagerOverride>(
          &mock_mahi_web_contents_manager);

  auto menu_widget = std::make_unique<ActiveWidget>();
  menu_widget->Init(CreateParamsForTestWidget());
  auto* menu_view =
      menu_widget->SetContentsView(std::make_unique<MahiMenuView>());

  auto event_generator = std::make_unique<ui::test::EventGenerator>(
      views::GetRootWindow(menu_widget.get()));
  event_generator->MoveMouseTo(menu_view->GetViewByID(ViewID::kTextfield)
                                   ->GetBoundsInScreen()
                                   .CenterPoint());
  event_generator->ClickLeftButton();
  TypeTestResponse(event_generator.get());

  event_generator->MoveMouseTo(
      menu_view->GetViewByID(ViewID::kSubmitQuestionButton)
          ->GetBoundsInScreen()
          .CenterPoint());

  base::HistogramTester histogram;
  histogram.ExpectBucketCount(kMahiContextMenuButtonClickHistogram,
                              MahiMenuButton::kSubmitQuestionButton, 0);

  // Make sure that clicking the summary button would trigger the function in
  // `MahiWebContentsManager` with the correct parameters.
  base::RunLoop run_loop;
  EXPECT_CALL(mock_mahi_web_contents_manager, OnContextMenuClicked)
      .WillOnce([&run_loop, &menu_widget](
                    int64_t display_id,
                    ::chromeos::mahi::ButtonType button_type,
                    const std::u16string& question,
                    const gfx::Rect& mahi_menu_bounds) {
        EXPECT_EQ(display::Screen::GetScreen()
                      ->GetDisplayNearestWindow(menu_widget->GetNativeWindow())
                      .id(),
                  display_id);
        EXPECT_EQ(::chromeos::mahi::ButtonType::kQA, button_type);
        EXPECT_EQ(u"test", question);
        run_loop.Quit();
      });

  event_generator->ClickLeftButton();
  run_loop.Run();

  histogram.ExpectBucketCount(kMahiContextMenuButtonClickHistogram,
                              MahiMenuButton::kSubmitQuestionButton, 1);
}

TEST_F(MahiMenuViewTest, EmptyQuestionNotSubmitted) {
  MockMahiWebContentsManager mock_mahi_web_contents_manager;
  auto scoped_mahi_web_contents_manager =
      std::make_unique<chromeos::ScopedMahiWebContentsManagerOverride>(
          &mock_mahi_web_contents_manager);

  auto menu_widget = std::make_unique<ActiveWidget>();
  menu_widget->Init(CreateParamsForTestWidget());
  auto* menu_view =
      menu_widget->SetContentsView(std::make_unique<MahiMenuView>());

  auto event_generator = std::make_unique<ui::test::EventGenerator>(
      views::GetRootWindow(menu_widget.get()));
  event_generator->MoveMouseTo(menu_view->GetViewByID(ViewID::kTextfield)
                                   ->GetBoundsInScreen()
                                   .CenterPoint());
  event_generator->ClickLeftButton();

  // Make sure that hitting enter with an empty textfield doesn't result in a
  // call to OnContextMenuClicked.
  base::RunLoop run_loop;
  EXPECT_CALL(mock_mahi_web_contents_manager, OnContextMenuClicked).Times(0);

  event_generator->PressAndReleaseKey(ui::VKEY_RETURN);
}

TEST_F(MahiMenuViewTest, AccessibleProperties) {
  auto menu_widget = std::make_unique<ActiveWidget>();
  menu_widget->Init(CreateParamsForTestWidget());
  auto* menu_view =
      menu_widget->SetContentsView(std::make_unique<MahiMenuView>());

  ui::AXNodeData data;
  menu_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kDialog);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            l10n_util::GetStringUTF16(IDS_ASH_MAHI_MENU_TITLE));
}

}  // namespace chromeos::mahi
