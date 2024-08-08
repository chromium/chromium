// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_ASH_INLINE_LOGIN_DIALOG_ONBOARDING_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_ASH_INLINE_LOGIN_DIALOG_ONBOARDING_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/signin/ash/inline_login_dialog.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
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

namespace ash {

// Inherits from InlineLoginDialog to handle the special scenario where
// the dialog is shown during onboarding.
class InlineLoginDialogOnboarding : public InlineLoginDialog {
 public:
  class Delegate : public views::WidgetObserver {
   public:
    explicit Delegate(InlineLoginDialogOnboarding* dialog);
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;
    ~Delegate() override;

    // Closes the dialog without making it executing the callback.
    void CloseWithoutCallback();

    void Close();

    void UpdateDialogBounds(const gfx::Rect& new_bounds);

    InlineLoginDialogOnboarding* dialog() { return dialog_; }

   private:
    // views::WidgetObserver:
    void OnWidgetClosing(views::Widget* widget) override;

    raw_ptr<InlineLoginDialogOnboarding> dialog_ = nullptr;
    raw_ptr<views::Widget> widget_ = nullptr;
  };

  static InlineLoginDialogOnboarding* Show(
      const gfx::Size& size,
      gfx::NativeWindow window,
      base::OnceCallback<void(void)> dialog_closed_callback);

 protected:
  // ui::WebDialogDelegate overrides
  ui::mojom::ModalType GetDialogModalType() const override;

 private:
  InlineLoginDialogOnboarding(
      const gfx::Size& bounds,
      base::OnceCallback<void(void)> dialog_closed_callback);
  ~InlineLoginDialogOnboarding() override;

  InlineLoginDialogOnboarding(const InlineLoginDialogOnboarding&) = delete;
  InlineLoginDialogOnboarding& operator=(const InlineLoginDialogOnboarding&) =
      delete;

  void UpdateDialogBounds(const gfx::Rect& bounds);

  void CloseDialog();

  void GetDialogSize(gfx::Size* size) const override;
  void AdjustWidgetInitParams(views::Widget::InitParams* params) override;
  void OnDialogClosed(const std::string& json_retval) override;

  gfx::Size size_;
  base::OnceCallback<void(void)> dialog_closed_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_ASH_INLINE_LOGIN_DIALOG_ONBOARDING_H_
