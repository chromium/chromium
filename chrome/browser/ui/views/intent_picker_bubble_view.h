// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INTENT_PICKER_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_INTENT_PICKER_BUBBLE_VIEW_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/link_capturing/intent_picker_info.h"
#include "chrome/browser/ui/views/location_bar/location_bar_bubble_delegate_view.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/gfx/image/image.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/controls/scroll_view.h"
#include "url/origin.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class Button;
class Checkbox;
class Widget;
}  // namespace views

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

class IntentPickerBubbleView : public LocationBarBubbleDelegateView {
  METADATA_HEADER(IntentPickerBubbleView, LocationBarBubbleDelegateView)

 public:
  using AppInfo = apps::IntentPickerAppInfo;
  using BubbleType = apps::IntentPickerBubbleType;

  // Unique identifiers for Views within the IntentPickerBubbleView hierarchy.
  enum ViewId {
    // The container for app selection buttons.
    kItemContainer = 1,
    // The "Remember my choice" checkbox.
    kRememberCheckbox,
  };

  IntentPickerBubbleView(views::View* anchor_view,
                         BubbleType bubble_type,
                         std::vector<AppInfo> app_info,
                         IntentPickerResponse intent_picker_cb,
                         content::WebContents* web_contents,
                         bool show_stay_in_chrome,
                         bool show_remember_selection,
                         const std::optional<url::Origin>& initiating_origin);

  IntentPickerBubbleView(const IntentPickerBubbleView&) = delete;
  IntentPickerBubbleView& operator=(const IntentPickerBubbleView&) = delete;

  ~IntentPickerBubbleView() override;

  static views::Widget* ShowBubble(
      views::View* anchor_view,
      views::Button* highlighted_button,
      BubbleType bubble_type,
      content::WebContents* web_contents,
      std::vector<AppInfo> app_info,
      bool show_stay_in_chrome,
      bool show_remember_selection,
      const std::optional<url::Origin>& initiating_origin,
      IntentPickerResponse intent_picker_cb);
  static IntentPickerBubbleView* intent_picker_bubble() {
    return intent_picker_bubble_;
  }

  static base::AutoReset<bool> SetAutoAcceptIntentPickerBubbleForTesting();

  static void CloseCurrentBubble();

  // LocationBarBubbleDelegateView overrides:
  bool ShouldShowCloseButton() const override;

  BubbleType bubble_type() const { return bubble_type_; }

  // Selects the default app for the current configuration. Must be called after
  // the Bubble is shown.
  void SelectDefaultItem();

  // Returns the index of the currently selected item. May return nullopt to
  // indicate no selection.
  std::optional<size_t> GetSelectedIndex() const;

  // A ScrollView which contains a list of apps. This view manages the selection
  // state for the dialog.
  class IntentPickerAppsView : public views::ScrollView {
    METADATA_HEADER(IntentPickerAppsView, views::ScrollView)

   public:
    virtual void SetSelectedIndex(std::optional<size_t> index) = 0;
    virtual std::optional<size_t> GetSelectedIndex() const = 0;
  };

  const std::vector<AppInfo>& app_info_for_testing() const { return app_info_; }

 protected:
  // LocationBarBubbleDelegateView overrides:
  std::u16string GetWindowTitle() const override;
  void CloseBubble() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(IntentPickerBubbleViewTest, WindowTitle);

  // views::BubbleDialogDelegateView overrides:
  void OnWidgetDestroying(views::Widget* widget) override;

  // Called when the app at |index| is selected in the app list. If
  // |accepted| is true, the dialog should be immediately accepted with that app
  // selected. If |index| is nullopt, no app is selected, and the Accept button
  // will be disabled
  void OnAppSelected(std::optional<size_t> index, bool accepted);

  void Initialize();

  void OnDialogAccepted();
  void OnDialogCancelled();
  void OnDialogClosed();

  // Runs |intent_picker_cb_| and closes the current bubble view.
  void RunCallbackAndCloseBubble(const std::string& launch_name,
                                 apps::PickerEntryType entry_type,
                                 apps::IntentPickerCloseReason close_reason,
                                 bool should_persist);

  // Returns true if this picker has candidates for the user to choose from, and
  // false otherwise. For instance, if Chrome was the only app candidate
  // provided, it will have been erased from |app_infos_| and this method would
  // return false.
  bool HasCandidates() const;

  // Updates whether the persistence checkbox is enabled or not.
  void UpdateCheckboxState(size_t index);

  // Clears this bubble from being considered the currently open bubble.
  void ClearIntentPickerBubbleView();

  static IntentPickerBubbleView* intent_picker_bubble_;

  // Callback used to respond to AppsNavigationThrottle.
  IntentPickerResponse intent_picker_cb_;

  std::vector<AppInfo> app_info_;

  raw_ptr<IntentPickerAppsView> apps_view_ = nullptr;

  raw_ptr<views::Checkbox> remember_selection_checkbox_ = nullptr;

  // When true, enables an alternate layout which presents apps as a grid
  // instead of a list.
  const bool use_grid_view_;

  // Tells whether 'Stay in Chrome' button should be shown or hidden.
  const bool show_stay_in_chrome_;

  // Whether 'Remember my choice' checkbox should be shown or hidden.
  const bool show_remember_selection_;

  // The type of bubble to show, used to customize some text and behavior.
  const BubbleType bubble_type_;

  // The origin initiating this picker.
  const std::optional<url::Origin> initiating_origin_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_INTENT_PICKER_BUBBLE_VIEW_H_
