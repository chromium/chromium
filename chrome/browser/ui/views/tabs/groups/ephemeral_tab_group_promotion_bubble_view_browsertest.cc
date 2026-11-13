// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/groups/ephemeral_tab_group_promotion_bubble_view.h"

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/groups/color_picker_view.h"
#include "chrome/browser/ui/views/tabs/groups/tab_group_editor_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/strings/grit/components_strings.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

class EphemeralTabGroupPromotionBubbleViewBrowserTest
    : public InProcessBrowserTest {
 public:
  EphemeralTabGroupPromotionBubbleViewBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kTabGroupsFocusing, features::kNonGroupFocus}, {});
  }

 protected:
  TabStripModel* tab_strip_model() { return browser()->GetTabStripModel(); }
  TabGroupModel* group_model() { return tab_strip_model()->group_model(); }

  // Enters non-group focus on tab 0 and opens the editor for the resulting
  // ephemeral group, which dispatches to the promotion bubble.
  tab_groups::TabGroupId EnterFocusAndOpenBubble() {
    const tab_groups::TabGroupId group = tab_strip_model()->EnterFocusMode({0});
    tab_strip_model()->OpenTabGroupEditor(group);
    return group;
  }

  ui::ElementContext browser_context() {
    return views::ElementTrackerViews::GetContextForView(
        BrowserView::GetBrowserViewForBrowser(browser()));
  }

  views::Widget* FindOpenBubbleWidget(ui::ElementIdentifier id) {
    for (views::View* view :
         views::ElementTrackerViews::GetInstance()->GetAllMatchingViews(
             id, browser_context())) {
      if (views::Widget* widget = view->GetWidget();
          widget && !widget->IsClosed()) {
        return widget;
      }
    }
    return nullptr;
  }

  views::Widget* WaitForBubbleWidget() {
    views::Widget* const widget =
        FindOpenBubbleWidget(EphemeralTabGroupPromotionBubbleView::
                                 kEphemeralTabGroupPromotionBubbleViewId);
    if (!widget) {
      return nullptr;
    }
    views::test::WidgetVisibleWaiter(widget).Wait();
    return widget;
  }

  template <typename ViewClass = views::View>
  static ViewClass* GetViewInBubble(views::Widget* widget,
                                    ui::ElementIdentifier id) {
    views::View* const view =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            id, views::ElementTrackerViews::GetContextForWidget(widget));
    return views::AsViewClass<ViewClass>(view);
  }

  static views::Textfield* GetTitleField(views::Widget* widget) {
    return GetViewInBubble<views::Textfield>(widget, kTabGroupEditorBubbleId);
  }

  static ColorPickerView* GetColorPicker(views::Widget* widget) {
    return views::AsViewClass<ColorPickerView>(widget->widget_delegate()
                                                   ->AsBubbleDialogDelegate()
                                                   ->GetContentsView()
                                                   ->children()[2]);
  }

  static void Click(views::View* view) {
    views::Button* const button = views::Button::AsButton(view);
    ASSERT_NE(nullptr, button);
    ASSERT_TRUE(button->GetVisible());
    ASSERT_TRUE(button->GetEnabled());
    views::test::ButtonTestApi(button).NotifyDefaultMouseClick();
  }

  static tab_groups::TabGroupColorId PickDifferentColor(
      views::Widget* widget,
      tab_groups::TabGroupColorId initial_color) {
    ColorPickerView* const color_picker = GetColorPicker(widget);
    EXPECT_NE(nullptr, color_picker);
    const TabGroupEditorBubbleView::Colors colors =
        ColorPickerView::GetTabGroupColors();
    size_t new_color_index = 0;
    while (colors[new_color_index].first == initial_color) {
      ++new_color_index;
    }
    Click(color_picker->GetElementAtIndexForTesting(
        static_cast<int>(new_color_index)));
    return colors[new_color_index].first;
  }

  // Dispatches a key press to the bubble. Routes through `widget->OnKeyEvent()`
  // when the widget holds OS activation; falls back to `title_field` when a
  // parallel browser_tests shard holds OS window activation so `FocusManager`
  // keeps `title_field` as the stored focus view.
  static void SendKeyToTitleField(views::Widget* widget,
                                  views::Textfield* title_field,
                                  ui::KeyboardCode key_code) {
    ui::KeyEvent event(ui::EventType::kKeyPressed, key_code, ui::EF_NONE);
    if (widget->IsActive()) {
      EXPECT_EQ(widget->GetFocusManager()->GetFocusedView(), title_field);
      widget->OnKeyEvent(&event);
    } else {
      EXPECT_EQ(widget->GetFocusManager()->GetStoredFocusView(), title_field);
      title_field->OnKeyEvent(&event);
    }
  }

  static void WaitForClose(base::WeakPtr<views::Widget> widget) {
    EXPECT_TRUE(base::test::RunUntil([&]() { return !widget; }));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Opening the editor for an ephemeral group shows the promotion bubble (not
// the regular editor) with a heading, title field, color picker and a
// Cancel / Create button row.
IN_PROC_BROWSER_TEST_F(EphemeralTabGroupPromotionBubbleViewBrowserTest,
                       ShowsPromotionBubbleForEphemeralGroup) {
  EnterFocusAndOpenBubble();
  views::Widget* const widget = WaitForBubbleWidget();
  ASSERT_NE(nullptr, widget);
  EXPECT_EQ(nullptr,
            FindOpenBubbleWidget(
                TabGroupEditorBubbleView::kTabGroupEditorBubbleViewId));

  views::BubbleDialogDelegate* const delegate =
      widget->widget_delegate()->AsBubbleDialogDelegate();
  EXPECT_EQ(delegate->buttons(),
            static_cast<int>(ui::mojom::DialogButton::kNone));

  views::View* const contents = delegate->GetContentsView();
  ASSERT_EQ(contents->children().size(), 4u);
  auto* const heading =
      views::AsViewClass<views::Label>(contents->children()[0]);
  ASSERT_NE(nullptr, heading);
  EXPECT_EQ(
      heading->GetText(),
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_CONVERT_TO_GROUP));
  views::Textfield* const title_field = GetTitleField(widget);
  EXPECT_EQ(title_field, contents->children()[1]);
  EXPECT_NE(nullptr,
            views::AsViewClass<ColorPickerView>(contents->children()[2]));

  auto* const cancel = GetViewInBubble<views::LabelButton>(
      widget, EphemeralTabGroupPromotionBubbleView::kCancelButtonId);
  auto* const create = GetViewInBubble<views::LabelButton>(
      widget, EphemeralTabGroupPromotionBubbleView::kCreateButtonId);
  ASSERT_NE(nullptr, cancel);
  ASSERT_NE(nullptr, create);
  EXPECT_EQ(cancel->GetText(), l10n_util::GetStringUTF16(IDS_CANCEL));
  EXPECT_EQ(create->GetText(), l10n_util::GetStringUTF16(
                                   IDS_TAB_GROUP_HEADER_CXMENU_CREATE_GROUP));

  // None of the regular editor menu items exist.
  EXPECT_EQ(
      nullptr,
      contents->GetViewByID(
          TabGroupEditorBubbleView::TAB_GROUP_HEADER_CXMENU_NEW_TAB_IN_GROUP));
  EXPECT_EQ(nullptr,
            contents->GetViewByID(
                TabGroupEditorBubbleView::TAB_GROUP_HEADER_CXMENU_UNGROUP));
  EXPECT_EQ(nullptr,
            contents->GetViewByID(
                TabGroupEditorBubbleView::TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP));

  EXPECT_EQ(delegate->GetInitiallyFocusedView(), title_field);
  EXPECT_EQ(widget->GetFocusManager()->GetStoredFocusView(), title_field);
  if (widget->IsActive()) {
    EXPECT_EQ(widget->GetFocusManager()->GetFocusedView(), title_field);
  }
}

IN_PROC_BROWSER_TEST_F(EphemeralTabGroupPromotionBubbleViewBrowserTest,
                       CancelDiscardsChangesAndKeepsGroupEphemeral) {
  const tab_groups::TabGroupId group = EnterFocusAndOpenBubble();
  views::Widget* const widget = WaitForBubbleWidget();
  ASSERT_NE(nullptr, widget);
  base::WeakPtr<views::Widget> weak_widget = widget->GetWeakPtr();

  const tab_groups::TabGroupColorId initial_color =
      group_model()->GetTabGroup(group)->visual_data()->color();
  GetTitleField(widget)->InsertOrReplaceText(u"Ephemeral Title");
  PickDifferentColor(widget, initial_color);

  // Edits are deferred until Create is pressed.
  EXPECT_EQ(group_model()->GetTabGroup(group)->visual_data()->title(), u"");
  EXPECT_EQ(group_model()->GetTabGroup(group)->visual_data()->color(),
            initial_color);

  Click(GetViewInBubble(widget,
                        EphemeralTabGroupPromotionBubbleView::kCancelButtonId));
  WaitForClose(weak_widget);

  EXPECT_TRUE(tab_strip_model()->IsEphemeralTabGroup(group));
  EXPECT_EQ(group, tab_strip_model()->GetFocusedGroup());
  EXPECT_EQ(group_model()->GetTabGroup(group)->visual_data()->title(), u"");
  EXPECT_EQ(group_model()->GetTabGroup(group)->visual_data()->color(),
            initial_color);
}

IN_PROC_BROWSER_TEST_F(EphemeralTabGroupPromotionBubbleViewBrowserTest,
                       CreatePromotesGroupWithChosenTitleAndColor) {
  const tab_groups::TabGroupId group = EnterFocusAndOpenBubble();
  views::Widget* const widget = WaitForBubbleWidget();
  ASSERT_NE(nullptr, widget);
  base::WeakPtr<views::Widget> weak_widget = widget->GetWeakPtr();

  GetTitleField(widget)->InsertOrReplaceText(u"Promoted Title");
  const tab_groups::TabGroupColorId initial_color =
      group_model()->GetTabGroup(group)->visual_data()->color();
  const tab_groups::TabGroupColorId chosen_color =
      PickDifferentColor(widget, initial_color);

  // The visuals must already be applied when observers hear about the
  // promotion, so session / sync / tab restore persist the right data.
  class PromotionObserver : public TabStripModelObserver {
   public:
    explicit PromotionObserver(TabGroupModel* model) : model_(model) {}
    void OnTabGroupPromoted(const tab_groups::TabGroupId& group_id) override {
      visuals_at_promotion = *model_->GetTabGroup(group_id)->visual_data();
    }
    raw_ptr<TabGroupModel> model_;
    std::optional<tab_groups::TabGroupVisualData> visuals_at_promotion;
  } observer(group_model());
  tab_strip_model()->AddObserver(&observer);

  Click(GetViewInBubble(widget,
                        EphemeralTabGroupPromotionBubbleView::kCreateButtonId));
  WaitForClose(weak_widget);
  tab_strip_model()->RemoveObserver(&observer);

  EXPECT_FALSE(tab_strip_model()->IsEphemeralTabGroup(group));
  // Promotion keeps the group focused; it is now a regular focused group.
  EXPECT_EQ(group, tab_strip_model()->GetFocusedGroup());
  ASSERT_TRUE(observer.visuals_at_promotion.has_value());
  EXPECT_EQ(observer.visuals_at_promotion->title(), u"Promoted Title");
  EXPECT_EQ(observer.visuals_at_promotion->color(), chosen_color);
  EXPECT_EQ(group_model()->GetTabGroup(group)->visual_data()->title(),
            u"Promoted Title");
  EXPECT_EQ(group_model()->GetTabGroup(group)->visual_data()->color(),
            chosen_color);
}

IN_PROC_BROWSER_TEST_F(EphemeralTabGroupPromotionBubbleViewBrowserTest,
                       EnterKeyPromotesGroup) {
  const tab_groups::TabGroupId group = EnterFocusAndOpenBubble();
  views::Widget* const widget = WaitForBubbleWidget();
  ASSERT_NE(nullptr, widget);
  base::WeakPtr<views::Widget> weak_widget = widget->GetWeakPtr();

  views::Textfield* const title_field = GetTitleField(widget);
  ASSERT_NE(nullptr, title_field);
  title_field->InsertOrReplaceText(u"Enter Title");
  SendKeyToTitleField(widget, title_field, ui::VKEY_RETURN);
  WaitForClose(weak_widget);

  EXPECT_FALSE(tab_strip_model()->IsEphemeralTabGroup(group));
  EXPECT_EQ(group_model()->GetTabGroup(group)->visual_data()->title(),
            u"Enter Title");
}

IN_PROC_BROWSER_TEST_F(EphemeralTabGroupPromotionBubbleViewBrowserTest,
                       EscapeKeyCancels) {
  const tab_groups::TabGroupId group = EnterFocusAndOpenBubble();
  views::Widget* const widget = WaitForBubbleWidget();
  ASSERT_NE(nullptr, widget);
  base::WeakPtr<views::Widget> weak_widget = widget->GetWeakPtr();

  const tab_groups::TabGroupColorId initial_color =
      group_model()->GetTabGroup(group)->visual_data()->color();
  views::Textfield* const title_field = GetTitleField(widget);
  ASSERT_NE(nullptr, title_field);
  title_field->InsertOrReplaceText(u"Discarded");
  PickDifferentColor(widget, initial_color);

  SendKeyToTitleField(widget, title_field, ui::VKEY_ESCAPE);
  WaitForClose(weak_widget);

  EXPECT_TRUE(tab_strip_model()->IsEphemeralTabGroup(group));
  EXPECT_EQ(group_model()->GetTabGroup(group)->visual_data()->title(), u"");
  EXPECT_EQ(group_model()->GetTabGroup(group)->visual_data()->color(),
            initial_color);
}

IN_PROC_BROWSER_TEST_F(EphemeralTabGroupPromotionBubbleViewBrowserTest,
                       EscapeAcceleratorCancelsWhenTitleFieldUnfocused) {
  const tab_groups::TabGroupId group = EnterFocusAndOpenBubble();
  views::Widget* const widget = WaitForBubbleWidget();
  ASSERT_NE(nullptr, widget);
  base::WeakPtr<views::Widget> weak_widget = widget->GetWeakPtr();

  const tab_groups::TabGroupColorId initial_color =
      group_model()->GetTabGroup(group)->visual_data()->color();
  GetTitleField(widget)->InsertOrReplaceText(u"Discarded");
  PickDifferentColor(widget, initial_color);

  // Clear focus so Escape routes through FocusManager -> DialogClientView's
  // accelerator handler rather than TextfieldController::HandleKeyEvent().
  widget->GetFocusManager()->ClearFocus();
  ui::KeyEvent escape(ui::EventType::kKeyPressed, ui::VKEY_ESCAPE, ui::EF_NONE);
  widget->OnKeyEvent(&escape);
  WaitForClose(weak_widget);

  EXPECT_TRUE(tab_strip_model()->IsEphemeralTabGroup(group));
  EXPECT_EQ(group_model()->GetTabGroup(group)->visual_data()->title(), u"");
  EXPECT_EQ(group_model()->GetTabGroup(group)->visual_data()->color(),
            initial_color);
}

IN_PROC_BROWSER_TEST_F(EphemeralTabGroupPromotionBubbleViewBrowserTest,
                       DeactivationDiscardsChangesAndKeepsGroupEphemeral) {
  const tab_groups::TabGroupId group = EnterFocusAndOpenBubble();
  views::Widget* const widget = WaitForBubbleWidget();
  ASSERT_NE(nullptr, widget);
  base::WeakPtr<views::Widget> weak_widget = widget->GetWeakPtr();

  const tab_groups::TabGroupColorId initial_color =
      group_model()->GetTabGroup(group)->visual_data()->color();
  GetTitleField(widget)->InsertOrReplaceText(u"Discarded");
  PickDifferentColor(widget, initial_color);

  widget->OnNativeWidgetActivationChanged(false);
  WaitForClose(weak_widget);

  EXPECT_TRUE(tab_strip_model()->IsEphemeralTabGroup(group));
  EXPECT_EQ(group, tab_strip_model()->GetFocusedGroup());
  EXPECT_EQ(group_model()->GetTabGroup(group)->visual_data()->title(), u"");
  EXPECT_EQ(group_model()->GetTabGroup(group)->visual_data()->color(),
            initial_color);
}

// Exiting focus mode removes the ephemeral group; a bubble that is still open
// for it must close rather than act on a group that no longer exists.
IN_PROC_BROWSER_TEST_F(EphemeralTabGroupPromotionBubbleViewBrowserTest,
                       ClosesWhenGroupIsRemoved) {
  // Need a second tab so the ephemeral group can be left by activating it.
  ASSERT_TRUE(AddTabAtIndex(1, GURL("about:blank"), ui::PAGE_TRANSITION_TYPED));
  tab_strip_model()->ActivateTabAt(0);

  const tab_groups::TabGroupId group = EnterFocusAndOpenBubble();
  views::Widget* const widget = WaitForBubbleWidget();
  ASSERT_NE(nullptr, widget);
  base::WeakPtr<views::Widget> weak_widget = widget->GetWeakPtr();

  tab_strip_model()->ExitFocusMode();
  // Removal may be deferred while the exit animates (see
  // TabStripModel::TakeEphemeralGroupRemovalLock()).
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !group_model()->ContainsTabGroup(group); }));
  WaitForClose(weak_widget);
}

// The promotion bubble is not the editor bubble: it must not record the
// editor's name/color changed or tab count metrics.
IN_PROC_BROWSER_TEST_F(EphemeralTabGroupPromotionBubbleViewBrowserTest,
                       CreateDoesNotRecordEditorBubbleMetrics) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  const tab_groups::TabGroupId group = EnterFocusAndOpenBubble();
  views::Widget* const widget = WaitForBubbleWidget();
  ASSERT_NE(nullptr, widget);
  base::WeakPtr<views::Widget> weak_widget = widget->GetWeakPtr();

  const tab_groups::TabGroupColorId initial_color =
      group_model()->GetTabGroup(group)->visual_data()->color();
  GetTitleField(widget)->InsertOrReplaceText(u"Promoted Title");
  PickDifferentColor(widget, initial_color);
  Click(GetViewInBubble(widget,
                        EphemeralTabGroupPromotionBubbleView::kCreateButtonId));
  WaitForClose(weak_widget);

  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "TabGroups_TabGroupBubble_NameChanged"));
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "TabGroups_TabGroupBubble_ColorChanged"));
  histogram_tester.ExpectTotalCount("TabGroups.ContextMenu.EditorBubble.Action",
                                    0);
  histogram_tester.ExpectTotalCount("TabGroups.TabGroupBubble.TabCount", 0);
}

IN_PROC_BROWSER_TEST_F(EphemeralTabGroupPromotionBubbleViewBrowserTest,
                       AdjustIfOffscreen) {
  browser()->GetWindow()->SetBounds(
      display::Screen::Get()
          ->GetDisplayNearestWindow(browser()->GetWindow()->GetNativeWindow())
          .work_area());

  const tab_groups::TabGroupId group = tab_strip_model()->EnterFocusMode({0});
  const gfx::Rect window_bounds =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->GetWidget()
          ->GetWindowBoundsInScreen();

  // Show the bubble near the top-left of the window. The frame arrow should
  // stay on the top-left without horizontal offset.
  std::unique_ptr<views::Widget> widget =
      EphemeralTabGroupPromotionBubbleView::Show(
          browser(), group,
          BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view(),
          gfx::Rect(window_bounds.x() + 50, window_bounds.y() + 50, 50, 30),
          /*stop_context_menu_propagation=*/false);
  views::test::WidgetVisibleWaiter(widget.get()).Wait();

  views::BubbleDialogDelegate* const bubble =
      widget->widget_delegate()->AsBubbleDialogDelegate();
  views::BubbleFrameView* const frame_view = bubble->GetBubbleFrameView();

  EXPECT_EQ(frame_view->GetArrow(), views::BubbleBorder::Arrow::TOP_LEFT);
  EXPECT_EQ(frame_view->bubble_border()->arrow_offset(), 0);
  EXPECT_TRUE(window_bounds.Contains(bubble->GetBubbleBounds()));

  // Move the anchor near the bottom-left of the window. The frame arrow should
  // flip vertically to the bottom-left.
  bubble->SetAnchorRect(
      gfx::Rect(window_bounds.x() + 50, window_bounds.bottom() - 50, 50, 30));
  EXPECT_EQ(frame_view->GetArrow(), views::BubbleBorder::Arrow::BOTTOM_LEFT);
  EXPECT_EQ(frame_view->bubble_border()->arrow_offset(), 0);
  EXPECT_TRUE(window_bounds.Contains(bubble->GetBubbleBounds()));

  // Move the anchor near the top-right of the window. Because the promotion
  // bubble uses `PreferredArrowAdjustment::kOffset`, the frame arrow remains
  // `TOP_LEFT` and offsets horizontally to stay inside the browser window.
  bubble->SetAnchorRect(
      gfx::Rect(window_bounds.right() - 80, window_bounds.y() + 50, 50, 30));
  EXPECT_EQ(frame_view->GetArrow(), views::BubbleBorder::Arrow::TOP_LEFT);
  EXPECT_NE(frame_view->bubble_border()->arrow_offset(), 0);
  EXPECT_TRUE(window_bounds.Contains(bubble->GetBubbleBounds()));

  // Move the anchor near the bottom-right of the window. The frame arrow flips
  // vertically to `BOTTOM_LEFT` and offsets horizontally.
  bubble->SetAnchorRect(gfx::Rect(window_bounds.right() - 80,
                                  window_bounds.bottom() - 50, 50, 30));
  EXPECT_EQ(frame_view->GetArrow(), views::BubbleBorder::Arrow::BOTTOM_LEFT);
  EXPECT_NE(frame_view->bubble_border()->arrow_offset(), 0);
  EXPECT_TRUE(window_bounds.Contains(bubble->GetBubbleBounds()));

  widget->CloseNow();
}

}  // namespace
