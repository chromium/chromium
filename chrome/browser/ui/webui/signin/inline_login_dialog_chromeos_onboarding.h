// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_DIALOG_CHROMEOS_ONBOARDING_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_DIALOG_CHROMEOS_ONBOARDING_H_

#include "chrome/browser/ui/webui/signin/inline_login_dialog_chromeos.h"

#include "base/callback.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}  // namespace views

namespace gfx {
class Rect;
}  // namespace gfx

namespace chromeos {

// Inherits from InlineLoginDialogChromeOS to handle the special scenario where
// the dialog is shown during onboarding.
class InlineLoginDialogChromeOSOnboarding : public InlineLoginDialogChromeOS {
 public:
  class Delegate : public views::WidgetObserver {
   public:
    explicit Delegate(InlineLoginDialogChromeOSOnboarding* dialog);
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    ~Delegate() override;

    // Closes the dialog without making it executing the callback.
    void CloseWithoutCallback();

    void Close();

    void UpdateDialogBounds(const gfx::Rect& new_bounds);

    InlineLoginDialogChromeOSOnboarding* dialog() { return dialog_; }

   private:
    // views::WidgetObserver:
    void OnWidgetClosing(views::Widget* widget) override;

    InlineLoginDialogChromeOSOnboarding* dialog_ = nullptr;
    views::Widget* widget_ = nullptr;
  };

  static InlineLoginDialogChromeOSOnboarding* Show(
      const gfx::Size& size,
      gfx::NativeWindow window,
      base::OnceCallback<void(void)> dialog_closed_callback);

 protected:
  // ui::WebDialogDelegate overrides
  ui::ModalType GetDialogModalType() const override;

 private:
  InlineLoginDialogChromeOSOnboarding(
      const gfx::Size& bounds,
      base::OnceCallback<void(void)> dialog_closed_callback);
  ~InlineLoginDialogChromeOSOnboarding() override;

  InlineLoginDialogChromeOSOnboarding(
      const InlineLoginDialogChromeOSOnboarding&) = delete;
  InlineLoginDialogChromeOSOnboarding& operator=(
      const InlineLoginDialogChromeOSOnboarding&) = delete;

  void UpdateDialogBounds(const gfx::Rect& bounds);

  void CloseDialog();

  void GetDialogSize(gfx::Size* size) const override;
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;
  void OnDialogClosed(const std::string& json_retval) override;

  gfx::Size size_;
  base::OnceCallback<void(void)> dialog_closed_callback_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_INLINE_LOGIN_DIALOG_CHROMEOS_ONBOARDING_H_
