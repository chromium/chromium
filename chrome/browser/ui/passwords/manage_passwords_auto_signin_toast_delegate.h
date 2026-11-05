// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_AUTO_SIGNIN_TOAST_DELEGATE_H_
#define CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_AUTO_SIGNIN_TOAST_DELEGATE_H_

#include <string>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/interaction/element_identifier.h"

namespace content {
class WebContents;
}

class BrowserWindowInterface;
class ToastController;
enum class ToastId;

// Delegate for handling actions from the auto sign-in toast.
class ManagePasswordsAutoSigninToastDelegate {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kOpenPasswordManagerSettingsMenuItem);

  static constexpr int kAutoSignInOpenPasswordManagerSettingsCommand = 1;

  explicit ManagePasswordsAutoSigninToastDelegate(
      content::WebContents* web_contents);
  ManagePasswordsAutoSigninToastDelegate(
      const ManagePasswordsAutoSigninToastDelegate&) = delete;
  ManagePasswordsAutoSigninToastDelegate& operator=(
      const ManagePasswordsAutoSigninToastDelegate&) = delete;
  virtual ~ManagePasswordsAutoSigninToastDelegate();

  void ExecuteCommand(int command_id, int event_flags);

  void SetOnToastClosedCallback(base::OnceClosure on_toast_closed_callback);

  void OnAutoSignInToast(const std::u16string& username);

 protected:
  virtual ToastController* GetToastController();
  virtual void NavigateToPasswordManagerSettings(
      BrowserWindowInterface* browser);

 private:
  void OnToastWidgetDestroyed(ToastId toast_id);

  base::WeakPtr<content::WebContents> web_contents_;
  base::OnceClosure on_toast_closed_callback_;
  base::CallbackListSubscription toast_observation_;
  base::WeakPtrFactory<ManagePasswordsAutoSigninToastDelegate>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_MANAGE_PASSWORDS_AUTO_SIGNIN_TOAST_DELEGATE_H_
