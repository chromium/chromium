// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INTENT_PICKER_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_INTENT_PICKER_BUBBLE_VIEW_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/apps/foundation/app_service/public/mojom/types.mojom.h"
#include "chrome/browser/chromeos/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/button.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class Checkbox;
class Widget;
}  // namespace views

namespace ui {
class Event;
}  // namespace ui

class IntentPickerLabelButton;

// A bubble that displays a list of applications (icons and names), after the
// list the UI displays a checkbox to allow the user remember the selection and
// after that a couple of buttons for either using the selected app or just
// staying in Chrome. The top right close button and clicking somewhere else
// outside of the bubble allows the user to dismiss the bubble (and stay in
// Chrome) without remembering any decision.
//
// This class communicates the user's selection with a callback supplied by
// AppsNavigationThrottle.
//   +--------------------------------+
//   | Open with                  [x] |
//   |                                |
//   | Icon1  Name1                   |
//   | Icon2  Name2                   |
//   |  ...                           |
//   | Icon(N) Name(N)                |
//   |                                |
//   | [_] Remember my choice         |
//   |                                |
//   |     [Use app] [Stay in Chrome] |
//   +--------------------------------+

class IntentPickerBubbleView : public LocationBarBubbleDelegateView,
                               public views::ButtonListener {
 public:
  using AppInfo = chromeos::IntentPickerAppInfo;

  ~IntentPickerBubbleView() override;
  static views::Widget* ShowBubble(views::View* anchor_view,
                                   content::WebContents* web_contents,
                                   std::vector<AppInfo> app_info,
                                   bool disable_stay_in_chrome,
                                   IntentPickerResponse intent_picker_cb);
  static std::unique_ptr<IntentPickerBubbleView> CreateBubbleView(
      std::vector<AppInfo> app_info,
      bool disable_stay_in_chrome,
      IntentPickerResponse intent_picker_cb,
      content::WebContents* web_contents);
  static IntentPickerBubbleView* intent_picker_bubble() {
    return intent_picker_bubble_;
  }
  static void CloseCurrentBubble();

  const std::vector<AppInfo>& GetAppInfoForTesting() const { return app_info_; }

  // LocationBarBubbleDelegateView overrides:
  bool Accept() override;
  bool Cancel() override;
  bool Close() override;
  bool ShouldShowCloseButton() const override;

 protected:
  // LocationBarBubbleDelegateView overrides:
  void Init() override;
  base::string16 GetWindowTitle() const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  void CloseBubble() override;

 private:
  friend class IntentPickerBubbleViewTest;
  FRIEND_TEST_ALL_PREFIXES(IntentPickerBubbleViewTest, NullIcons);
  FRIEND_TEST_ALL_PREFIXES(IntentPickerBubbleViewTest, NonNullIcons);
  FRIEND_TEST_ALL_PREFIXES(IntentPickerBubbleViewTest, LabelsPtrVectorSize);
  FRIEND_TEST_ALL_PREFIXES(IntentPickerBubbleViewTest, VerifyStartingInkDrop);
  FRIEND_TEST_ALL_PREFIXES(IntentPickerBubbleViewTest, InkDropStateTransition);
  FRIEND_TEST_ALL_PREFIXES(IntentPickerBubbleViewTest, PressButtonTwice);
  FRIEND_TEST_ALL_PREFIXES(IntentPickerBubbleViewTest, ChromeNotInCandidates);
  FRIEND_TEST_ALL_PREFIXES(IntentPickerBubbleViewTest, StayInChromeTest);
  FRIEND_TEST_ALL_PREFIXES(IntentPickerBubbleViewTest, WebContentsTiedToBubble);
  IntentPickerBubbleView(std::vector<AppInfo> app_info,
                         IntentPickerResponse intent_picker_cb,
                         content::WebContents* web_contents,
                         bool disable_display_in_chrome);

  // views::BubbleDialogDelegateView overrides:
  void OnWidgetDestroying(views::Widget* widget) override;

  // views::ButtonListener overrides:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Similar to ButtonPressed, except this controls the up/down/right/left input
  // while focusing on the |scroll_view_|.
  void ArrowButtonPressed(int index);

  // ui::EventHandler overrides:
  void OnKeyEvent(ui::KeyEvent* event) override;

  // Retrieves the IntentPickerLabelButton* contained at position |index| from
  // the internal ScrollView.
  IntentPickerLabelButton* GetIntentPickerLabelButtonAt(size_t index);
  void RunCallback(const std::string& launch_name,
                   apps::mojom::AppType app_type,
                   chromeos::IntentPickerCloseReason close_reason,
                   bool should_persist);

  // Returns true if this picker has candidates for the user to choose from, and
  // false otherwise. For instance, if Chrome was the only app candidate
  // provided, it will have been erased from |app_infos_| and this method would
  // return false.
  bool HasCandidates() const;

  // Accessory for |scroll_view_|'s contents size.
  size_t GetScrollViewSize() const;

  // Ensure the selected app is within the visible region of the ScrollView.
  void AdjustScrollViewVisibleRegion();

  // Set the new app selection, use the |event| (if provided) to show a more
  // accurate ripple effect w.r.t. the user's input.
  void SetSelectedAppIndex(int index, const ui::Event* event);

  // Calculate the next app to select given the current selection and |delta|.
  size_t CalculateNextAppIndex(int delta);

  // Updates whether the persistence checkbox is enabled or not.
  void UpdateCheckboxState();

  gfx::ImageSkia GetAppImageForTesting(size_t index);
  views::InkDropState GetInkDropStateForTesting(size_t);
  void PressButtonForTesting(size_t index, const ui::Event& event);

  static IntentPickerBubbleView* intent_picker_bubble_;

  // Callback used to respond to AppsNavigationThrottle.
  IntentPickerResponse intent_picker_cb_;

  // Pre-select the first app on the list.
  size_t selected_app_tag_ = 0;

  views::ScrollView* scroll_view_;

  std::vector<AppInfo> app_info_;

  views::Checkbox* remember_selection_checkbox_;

  // Tells whether or not 'Stay in Chrome' should be enabled as an option.
  const bool disable_stay_in_chrome_;

  DISALLOW_COPY_AND_ASSIGN(IntentPickerBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INTENT_PICKER_BUBBLE_VIEW_H_
