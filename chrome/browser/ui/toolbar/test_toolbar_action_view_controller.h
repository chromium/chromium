// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_ACTION_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_ACTION_VIEW_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_context_menu_model.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"

// A minimalistic and configurable ToolbarActionViewController for use in
// testing.
class TestToolbarActionViewController : public ToolbarActionViewController {
 public:
  explicit TestToolbarActionViewController(const std::string& id);

  TestToolbarActionViewController(const TestToolbarActionViewController&) =
      delete;
  TestToolbarActionViewController& operator=(
      const TestToolbarActionViewController&) = delete;

  ~TestToolbarActionViewController() override;

  // ToolbarActionViewController:
  std::string GetId() const override;
  void SetDelegate(ToolbarActionViewDelegate* delegate) override;
  ui::ImageModel GetIcon(content::WebContents* web_contents,
                         const gfx::Size& size) override;
  std::u16string GetActionName() const override;
  std::u16string GetActionTitle(
      content::WebContents* web_contents) const override;
  std::u16string GetAccessibleName(
      content::WebContents* web_contents) const override;
  std::u16string GetTooltip(content::WebContents* web_contents) const override;
  ToolbarActionViewController::HoverCardState GetHoverCardState(
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
  void UpdateState() override;
  extensions::SitePermissionsHelper::SiteInteraction GetSiteInteraction(
      content::WebContents* web_contents) const override;

  // Instruct the controller to fake showing a popup.
  void ShowPopup(bool by_user);

  // Configure the test controller. These also call UpdateDelegate().
  void SetActionName(const std::u16string& name);
  void SetActionTitle(const std::u16string& title);
  void SetAccessibleName(const std::u16string& name);
  void SetTooltip(const std::u16string& tooltip);
  void SetEnabled(bool is_enabled);

  int execute_action_count() const { return execute_action_count_; }

 private:
  // Updates the delegate, if one exists.
  void UpdateDelegate();

  // The id of the controller.
  std::string id_;

  // The delegate of the controller, if one exists.
  raw_ptr<ToolbarActionViewDelegate> delegate_ = nullptr;

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

#endif  // CHROME_BROWSER_UI_TOOLBAR_TEST_TOOLBAR_ACTION_VIEW_CONTROLLER_H_
