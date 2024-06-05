// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EXCLUSIVE_ACCESS_PERMISSION_PROMPT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EXCLUSIVE_ACCESS_PERMISSION_PROMPT_VIEW_H_

#include <string>

#include "chrome/browser/ui/views/permissions/exclusive_access_permission_prompt.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_base_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/flex_layout_view.h"

class Browser;

// The view for a prompt for a set of exclusive access (keyboard/pointer lock)
// permission requests, shown by `ExclusiveAccessPermissionPrompt`.
class ExclusiveAccessPermissionPromptView : public PermissionPromptBaseView {
  METADATA_HEADER(ExclusiveAccessPermissionPromptView, PermissionPromptBaseView)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAlwaysAllowId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAllowThisTimeId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kNeverAllowId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMainViewId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kLabelViewId1);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kLabelViewId2);

  ExclusiveAccessPermissionPromptView(
      Browser* browser,
      base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate);
  ExclusiveAccessPermissionPromptView(
      const ExclusiveAccessPermissionPromptView&) = delete;
  ExclusiveAccessPermissionPromptView& operator=(
      const ExclusiveAccessPermissionPromptView&) = delete;
  ~ExclusiveAccessPermissionPromptView() override;

  void Show();
  void UpdateAnchor(views::Widget* widget);
  void PrepareToClose();

  // PermissionPromptBaseView:
  bool ShouldShowCloseButton() const override;
  void Init() override;
  void AddedToWidget() override;
  std::u16string GetAccessibleWindowTitle() const override;
  std::u16string GetWindowTitle() const override;
  void RunButtonCallback(int type) override;

 private:
  friend class ExclusiveAccessPermissionPromptInteractiveTest;

  enum class ButtonType {
    kAlwaysAllow = 0,
    kAllowThisTime = 1,
    kNeverAllow = 2,
  };

  static int GetViewId(ButtonType button) { return static_cast<int>(button); }
  static ButtonType GetButtonType(int button_id) {
    return static_cast<ButtonType>(button_id);
  }

  void CreateWidget();
  void ShowWidget();
  void AddRequestLine(raw_ptr<const gfx::VectorIcon> icon,
                      const std::u16string& message,
                      std::size_t index);
  void InitButtons();
  void AddButton(views::View& buttons_container,
                 const std::u16string& label,
                 ButtonType type,
                 ui::ButtonStyle style,
                 ui::ElementIdentifier identifier);
  void AddAlwaysAllowButton(views::View& buttons_container);
  void AddAllowThisTimeButton(views::View& buttons_container);
  void ClosingPermission();

  const raw_ptr<Browser> browser_;
  base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EXCLUSIVE_ACCESS_PERMISSION_PROMPT_VIEW_H_
