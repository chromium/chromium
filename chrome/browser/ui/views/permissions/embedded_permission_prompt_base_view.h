// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_BASE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_BASE_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/permissions/permission_prompt_base_view.h"
#include "components/permissions/permission_prompt.h"
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
  class Delegate {
   public:
    virtual void Allow() = 0;
    virtual void AllowThisTime() = 0;
    virtual void Dismiss() = 0;
    virtual void Acknowledge() = 0;
    virtual void StopAllowing() = 0;
    virtual base::WeakPtr<permissions::PermissionPrompt::Delegate>
    GetPermissionPromptDelegate() const = 0;
    const std::vector<permissions::PermissionRequest*>& Requests() const;
  };

  EmbeddedPermissionPromptBaseView(Browser* browser,
                                   base::WeakPtr<Delegate> delegate);
  EmbeddedPermissionPromptBaseView(const EmbeddedPermissionPromptBaseView&) =
      delete;
  EmbeddedPermissionPromptBaseView& operator=(
      const EmbeddedPermissionPromptBaseView&) = delete;
  ~EmbeddedPermissionPromptBaseView() override;

  void Show();
  void UpdateAnchorPosition();
  void ShowWidget();
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
  };

  static int GetViewId(ButtonType button) { return static_cast<int>(button); }
  static ButtonType GetButtonType(int button_id) {
    return static_cast<ButtonType>(button_id);
  }

  // Configuration that needs to be implemented by subclasses
  virtual std::vector<RequestLineConfiguration> GetRequestLinesConfiguration()
      const = 0;
  virtual std::vector<ButtonConfiguration> GetButtonsConfiguration() const = 0;

  base::WeakPtr<Delegate>& delegate() { return delegate_; }
  const base::WeakPtr<Delegate>& delegate() const { return delegate_; }

 private:
  void CreateWidget();
  void AddRequestLine(const RequestLineConfiguration& line, std::size_t index);
  void AddButton(views::View& buttons_container,
                 const ButtonConfiguration& button);

  const raw_ptr<Browser> browser_;
  base::WeakPtr<Delegate> delegate_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_EMBEDDED_PERMISSION_PROMPT_BASE_VIEW_H_
