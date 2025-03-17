// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNOUT_CONFIRMATION_SIGNOUT_CONFIRMATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNOUT_CONFIRMATION_SIGNOUT_CONFIRMATION_HANDLER_H_

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/signin/chrome_signout_confirmation_prompt.h"
#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation.mojom.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Browser;

class SignoutConfirmationHandler
    : public signout_confirmation::mojom::PageHandler {
 public:
  // Initializes the handler with the mojo handlers and the needed information
  // to be displayed as well as callbacks to the main native view.
  SignoutConfirmationHandler(
      mojo::PendingReceiver<signout_confirmation::mojom::PageHandler> receiver,
      mojo::PendingRemote<signout_confirmation::mojom::Page> page,
      Browser* browser,
      ChromeSignoutConfirmationPromptVariant variant,
      SignoutConfirmationCallback callback);
  ~SignoutConfirmationHandler() override;

  SignoutConfirmationHandler(const SignoutConfirmationHandler&) = delete;
  SignoutConfirmationHandler& operator=(const SignoutConfirmationHandler&) =
      delete;

  // signout_confirmation::mojom::PageHandler:
  void UpdateViewHeight(uint32_t height) override;
  void Accept(bool uninstall_account_extensions) override;
  void Cancel(bool uninstall_account_extensions) override;
  void Close() override;

 private:
  // Run `completion_callback_` with the given `choice` and `bool
  // uninstall_account_extensions`, and close the dialog if there is one open.
  void FinishAndCloseDialog(ChromeSignoutConfirmationChoice choice,
                            bool uninstall_account_extensions);

  // Same as the below version except there are no `ExtensionInfoPtr` to send.
  void ComputeAndSendSignoutConfirmationDataWithoutExtensions();

  // Computes the initial `SignoutConfirmationData` given the list of
  // `account_extensions_info` and sends it to `page_` once complete.
  void ComputeAndSendSignoutConfirmationData(
      std::vector<signout_confirmation::mojom::ExtensionInfoPtr>
          account_extensions_info);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Computes info for all account extensions that need to be sent to `page_`.
  // Calls ComputeAndSendSignoutConfirmationData() once complete.
  void ComputeAccountExtensions();
#endif

  base::WeakPtr<Browser> browser_;

  // The variant of the signout confirmation prompt. This affects which actions
  // are taken when the user accepts or cancels the prompt, and the strings that
  // are displayed inside the prompt itself.
  ChromeSignoutConfirmationPromptVariant variant_;

  // Called when the user accepts, cancels or closes the prompt.
  SignoutConfirmationCallback completion_callback_;

  // Allows handling received messages from the web ui page.
  mojo::Receiver<signout_confirmation::mojom::PageHandler> receiver_;
  // Interface to send information to the web ui page.
  mojo::Remote<signout_confirmation::mojom::Page> page_;

  base::WeakPtrFactory<SignoutConfirmationHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNOUT_CONFIRMATION_SIGNOUT_CONFIRMATION_HANDLER_H_
