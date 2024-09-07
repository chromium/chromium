// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_HTTP_AUTH_DIALOG_HTTP_AUTH_DIALOG_H_
#define CHROMEOS_ASH_COMPONENTS_HTTP_AUTH_DIALOG_HTTP_AUTH_DIALOG_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "content/public/browser/login_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace ash {

// HTTP authentication is a feature that gates network resources behind a
// plaintext username/password ACL. This is typically implemented by
// web-browsers by showing a dialog to users part-way through a navigation with
// username/password textfields.
// https://developer.mozilla.org/en-US/docs/Web/HTTP/Authentication
//
// There are some pieces of UI in ash-chrome unrelated to web-browsing that
// expect fine grained control over the dialog that is shown. This class
// provides a mechanism to do so.

class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_HTTP_AUTH_DIALOG)
    HttpAuthDialog : public content::LoginDelegate {
 public:
  // There are two ways for this class to be deleted.
  // (1) The //content layer can decide the class is no longer necessary, in
  // which case the destructor will be directly invoked. In this case, clear
  // `callback_` and close the widget.
  // (2) The widget can be closed by the user. In this case, invoke the
  // `callback_`. This will cause (1) to trigger.
  ~HttpAuthDialog() override;

  class ScopedEnabler {
   public:
    ScopedEnabler();
    ~ScopedEnabler();
    ScopedEnabler(const ScopedEnabler&) = delete;
    ScopedEnabler& operator=(const ScopedEnabler&) = delete;
  };
  // Prior to shipping Lacros, ash-chrome needs to handle both browser-based
  // http-auth dialogs, and OS-based http-auth dialogs. Classes that need the
  // latter should call this method and keep the returned ScopedEnabler alive.
  // This forces the latter use-case.
  // After shipping Lacros, this method will be unnecessary as the OS-based
  // http-auth dialog will be the only remaining use case.
  static std::unique_ptr<ScopedEnabler> Enable();
  static bool IsEnabled();

  static std::unique_ptr<HttpAuthDialog> Create(
      const net::AuthChallengeInfo& auth_info,
      content::WebContents* web_contents,
      const GURL& url,
      LoginAuthRequiredCallback auth_required_callback);

  class Observer : public base::CheckedObserver {
   public:
    // Called when the dialog is shown.
    virtual void HttpAuthDialogShown(content::WebContents* web_contents) = 0;

    // Called when the dialog is cancelled. This can happen if the user presses
    // the "cancel" button, or if the network request is cancelled.
    virtual void HttpAuthDialogCancelled(
        content::WebContents* web_contents) = 0;

    // Called when the user presses the "continue" button. There is no guarantee
    // that the username/password are valid.
    virtual void HttpAuthDialogSupplied(content::WebContents* web_contents) = 0;
  };

  static void AddObserver(Observer* observer);
  static void RemoveObserver(Observer* observer);

  // Exposed for testing.
  static std::vector<HttpAuthDialog*> GetAllDialogsForTest();

  void SupplyCredentialsForTest(std::u16string_view username,
                                std::u16string_view password);
  void CancelForTest();

 private:
  // A basic view with username/password text fields.
  class DialogView : public views::View {
   public:
    DialogView(std::u16string_view authority, std::u16string_view explanation);
    ~DialogView() override;

    // Intentionally return by copy so that the return value can be used even if
    // DialogView is destroyed.
    std::u16string GetUsername() const;
    std::u16string GetPassword() const;

    void SetCredentialsForTest(std::u16string_view username,
                               std::u16string_view password);

    views::View* GetInitiallyFocusedView();

   private:
    // Non-owning refs to the input text fields.
    raw_ptr<views::Textfield> username_field_;
    raw_ptr<views::Textfield> password_field_;
  };

  // The constructor creates and shows a dialog.
  HttpAuthDialog(const net::AuthChallengeInfo& auth_info,
                 content::WebContents* web_contents,
                 const GURL& url,
                 LoginAuthRequiredCallback auth_required_callback);

  // In the production use-case, this method is called by views when the user
  // clicks the OK button. The dialog is in the process of closing. This method
  // merely needs to invoke `callback_`.
  // When this method is called from tests, the dialog is not in the process of
  // closing. Calling this method will invoke `callback_`, which will result in
  // destruction of this object, which will close the dialog.
  void SupplyCredentials(std::u16string_view username,
                         std::u16string_view password);

  // Similar to `SupplyCredentials` except this is the path for clicking the
  // cancel button or otherwise dismissing the dialog.
  void Cancel();

  static void NotifyShownAsync(content::WebContents* web_contents);
  static void NotifySuppliedAsync(content::WebContents* web_contents);
  static void NotifyCancelledAsync(content::WebContents* web_contents);

  net::AuthChallengeInfo auth_info_;

  // This class is owned by the //content layer. The only way to delete this
  // class is to invoke this callback.
  LoginAuthRequiredCallback callback_;

  // Handles configuration and callbacks from the dialog.
  views::DialogDelegate dialog_delegate_;

  // `dialog_view_` is owned by the views framework. The dialog is showing if
  // and only if these members are not `nullptr`. dialog_widget_ is created in
  // the constructor and destroyed in the destructor, so this means that the
  // lifetime of the dialog corresponds exactly to the lifetime of this class.
  raw_ptr<DialogView> dialog_view_ = nullptr;
  std::unique_ptr<views::Widget> dialog_widget_;

  // Tracks the WebContents instance that is showing the dialog.
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  base::WeakPtrFactory<HttpAuthDialog> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_HTTP_AUTH_DIALOG_HTTP_AUTH_DIALOG_H_
