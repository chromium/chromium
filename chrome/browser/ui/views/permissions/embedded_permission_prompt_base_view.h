// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_BASE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_BASE_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/permissions/embedded_permission_prompt_view_delegate.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_base_view.h"
#include "components/permissions/permission_prompt.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/vector_icon_types.h"

class Browser;

// Base view used for embedded permission prompts dialogs.
// It looks like this:
//
// |-------------------------------------------------|
// |                                              [X]|
// | Title (optional)                                |
// |-------------------------------------------------|
// |Icon1(optional)|RequestMessage1                  |
// |Icon2(optional)|RequestMessage2                  |
// |...                                              |
// |-------------------------------------------------|
// |                 Button1                         |
// |                 Button2                         |
// |                 ...                             |
// |-------------------------------------------------|

// Subclasses need to implement at least:
// GetRequestLinesConfiguration - provides the info for the main body of the
// prompt.
// GetButtonsConfiguration - provides the info for the buttons section.
// RunButtonCallback - called when a button is pressed.
// GetWindowTitle/GetAccessibleWindowTitle - inherited from
// views::BubbleDialogDelegateView.

class EmbeddedPermissionPromptBaseView : public PermissionPromptBaseView {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kMainViewId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kLabelViewId1);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kLabelViewId2);

  EmbeddedPermissionPromptBaseView(
      Browser* browser,
      base::WeakPtr<EmbeddedPermissionPromptViewDelegate> delegate);
  EmbeddedPermissionPromptBaseView(const EmbeddedPermissionPromptBaseView&) =
      delete;
  EmbeddedPermissionPromptBaseView& operator=(
      const EmbeddedPermissionPromptBaseView&) = delete;
  ~EmbeddedPermissionPromptBaseView() override;

  void Show();
  void UpdateAnchor(views::Widget* widget);
  void ClosingPermission();
  void PrepareToClose();

  // views::BubbleDialogDelegateView:
  bool ShouldShowCloseButton() const override;
  void Init() override;

 protected:
  enum class ButtonType {
    kAllow = 0,
    kPolicyOK = 1,
    kContinueAllowing = 2,
    kStopAllowing = 3,
    kClose = 4,
    kAllowThisTime = 5,
    kContinueNotAllowing = 6,
    kSystemSettings = 7,
  };

  struct RequestLineConfiguration {
    const raw_ptr<const gfx::VectorIcon> icon;
    std::u16string message;
  };

  struct ButtonConfiguration {
    std::u16string label;
    ButtonType type;
    ui::ButtonStyle style;
    ui::ElementIdentifier identifier;
  };

  static int GetViewId(ButtonType button) { return static_cast<int>(button); }
  static ButtonType GetButtonType(int button_id) {
    return static_cast<ButtonType>(button_id);
  }

  // Configuration that needs to be implemented by subclasses
  virtual std::vector<RequestLineConfiguration> GetRequestLinesConfiguration()
      const = 0;
  virtual std::vector<ButtonConfiguration> GetButtonsConfiguration() const = 0;

  base::WeakPtr<EmbeddedPermissionPromptViewDelegate>& delegate() {
    return delegate_;
  }
  const base::WeakPtr<EmbeddedPermissionPromptViewDelegate>& delegate() const {
    return delegate_;
  }

 private:
  void CreateWidget();
  void ShowWidget();
  void AddRequestLine(const RequestLineConfiguration& line, std::size_t index);
  void AddButton(views::View& buttons_container,
                 const ButtonConfiguration& button);

  const raw_ptr<Browser> browser_;
  base::WeakPtr<EmbeddedPermissionPromptViewDelegate> delegate_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_BASE_VIEW_H_
