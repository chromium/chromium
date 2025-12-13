// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_ACTION_VIEW_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_ACTION_VIEW_MODEL_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"

// A minimalistic and configurable ToolbarActionViewModel for use in
// testing.
class TestToolbarActionViewModel : public ToolbarActionViewModel {
 public:
  explicit TestToolbarActionViewModel(const std::string& id);

  TestToolbarActionViewModel(const TestToolbarActionViewModel&) = delete;
  TestToolbarActionViewModel& operator=(const TestToolbarActionViewModel&) =
      delete;

  ~TestToolbarActionViewModel() override;

  // ToolbarActionViewModel:
  std::string GetId() const override;
  base::CallbackListSubscription RegisterUpdateObserver(
      base::RepeatingClosure observer) override;
  ui::ImageModel GetIcon(content::WebContents* web_contents,
                         const gfx::Size& size) override;
  std::u16string GetActionName() const override;
  std::u16string GetActionTitle(
      content::WebContents* web_contents) const override;
  std::u16string GetAccessibleName(
      content::WebContents* web_contents) const override;
  std::u16string GetTooltip(content::WebContents* web_contents) const override;
  ToolbarActionViewModel::HoverCardState GetHoverCardState(
      content::WebContents* web_contents) const override;
  bool IsEnabled(content::WebContents* web_contents) const override;
  bool IsShowingPopup() const override;
  void HidePopup() override;
  gfx::NativeView GetPopupNativeView() override;
  ui::MenuModel* GetContextMenu(
      extensions::ExtensionContextMenuModel::ContextMenuSource
          context_menu_source) override;
  void ExecuteUserAction(InvocationSource source) override;
  void TriggerPopupForAPI(ShowPopupCallback callback) override;
  extensions::SitePermissionsHelper::SiteInteraction GetSiteInteraction(
      content::WebContents* web_contents) const override;

  // Instruct the controller to fake showing a popup.
  void ShowPopup(bool by_user);

  // Configure the test controller. These also call NotifyObserver().
  void SetActionName(const std::u16string& name);
  void SetActionTitle(const std::u16string& title);
  void SetAccessibleName(const std::u16string& name);
  void SetTooltip(const std::u16string& tooltip);
  void SetEnabled(bool is_enabled);

  int execute_action_count() const { return execute_action_count_; }

 private:
  // Notifies the observers.
  void NotifyObservers();

  // The id of the controller.
  std::string id_;

  // The observers of the view model.
  base::RepeatingClosureList observers_;

  // Action name for the controller.
  std::u16string action_name_;

  // Action title for the controller.
  std::u16string action_title_;

  // The optional accessible name and tooltip; by default these are empty.
  std::u16string accessible_name_;
  std::u16string tooltip_;

  // Whether or not the action is enabled.
  bool is_enabled_ = true;

  // The number of times the action would have been executed.
  int execute_action_count_ = 0;

  // True if a popup is (supposedly) currently showing.
  bool popup_showing_ = false;
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_ACTION_VIEW_MODEL_H_
