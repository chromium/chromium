// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNOUT_CONFIRMATION_SIGNOUT_CONFIRMATION_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNOUT_CONFIRMATION_SIGNOUT_CONFIRMATION_UI_H_

#include "base/functional/callback.h"
#include "chrome/browser/ui/signin/chrome_signout_confirmation_prompt.h"
#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace content {
class WebUI;
}  // namespace content

namespace ui {
class ColorChangeHandler;
}  // namespace ui

class Browser;
class SignoutConfirmationHandler;
class SignoutConfirmationUI;

class SignoutConfirmationUIConfig
    : public content::DefaultWebUIConfig<SignoutConfirmationUI> {
 public:
  SignoutConfirmationUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUISignoutConfirmationHost) {}
};

class SignoutConfirmationUI
    : public ui::MojoWebUIController,
      public signout_confirmation::mojom::PageHandlerFactory {
 public:
  explicit SignoutConfirmationUI(content::WebUI* web_ui);
  ~SignoutConfirmationUI() override;

  SignoutConfirmationUI(const SignoutConfirmationUI&) = delete;
  SignoutConfirmationUI& operator=(const SignoutConfirmationUI&) = delete;

  // Prepares the information to be given to the handler once ready.
  void Initialize(Browser* browser,
                  ChromeSignoutConfirmationPromptVariant variant,
                  SignoutConfirmationCallback callback);

  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          pending_receiver);

  // Instantiates the implementor of the
  // `signout_confirmation::mojom::PageHandlerFactory` mojo interface passing
  // the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<signout_confirmation::mojom::PageHandlerFactory>
          receiver);

  // Returns the instance of this class for the provided `contents` or nullptr
  // if `contents` does not display the signout confirmation UI.
  static SignoutConfirmationUI* GetForTesting(content::WebContents* contents);

  // Simulates accepting the signout confirmation dialog through a direct call
  // to the `handler_`.
  void AcceptDialogForTesting();

  // Simulates cancelling the signout confirmation dialog through a direct call
  // to the `handler_`.
  void CancelDialogForTesting();

 private:
  // signout_confirmation::mojom::SignoutConfirmationFactory:
  void CreateSignoutConfirmationHandler(
      mojo::PendingRemote<signout_confirmation::mojom::Page> page,
      mojo::PendingReceiver<signout_confirmation::mojom::PageHandler> receiver)
      override;

  // Callback awaiting `CreateSignoutConfirmationHandler` to create the handlers
  // with all the needed information to display.
  void OnMojoHandlersReady(
      Browser* browser,
      ChromeSignoutConfirmationPromptVariant variant,
      SignoutConfirmationCallback callback,
      mojo::PendingRemote<signout_confirmation::mojom::Page> page,
      mojo::PendingReceiver<signout_confirmation::mojom::PageHandler> receiver);

  // Callback that temporarily holds the information to be passed onto the
  // handler. The callback is called once the mojo handlers are available.
  base::OnceCallback<void(
      mojo::PendingRemote<signout_confirmation::mojom::Page>,
      mojo::PendingReceiver<signout_confirmation::mojom::PageHandler>)>
      initialize_handler_callback_;

  // Handler that notifies WebUI to fetch new stylesheets containing color
  // variables if the color provider changes.
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  // Handler implementing Mojo interface to communicate with the WebUI.
  std::unique_ptr<SignoutConfirmationHandler> handler_;

  mojo::Receiver<signout_confirmation::mojom::PageHandlerFactory>
      page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNOUT_CONFIRMATION_SIGNOUT_CONFIRMATION_UI_H_
