// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_UI_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/ui/webui/intro/finish_or_continue_handler.h"
#include "chrome/browser/ui/webui/intro/intro.mojom.h"
#include "chrome/browser/ui/webui/intro/intro_handler.h"
#include "chrome/browser/ui/webui/intro/sign_in_celebration_handler.h"
#include "chrome/browser/ui/webui/intro/sign_in_promo.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class IntroUI;
class SignInPromoHandler;

enum class IntroChoice {
  kContinueWithAccount,
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  kContinueWithoutAccount,
#endif
  kQuit,
};

// This is also used for logging, so do not remove or reorder existing entries.
enum class DefaultBrowserChoice {
  // The user clicked to set Chrome as their default browser.
  kClickSetAsDefault = 0,
  // The user skipped the prompt to set Chrome as their default browser.
  kSkip = 1,
  // The user exited the first run flow while on the prompt to set Chrome as
  // their default browser.
  kQuit = 2,
  // The prompt was not shown due to a timeout when checking if the browser is
  // already default.
  kNotShownOnTimeout = 3,
  // Chrome was successfully set as default browser.
  kSuccessfullySetAsDefault = 4,
  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kSuccessfullySetAsDefault
};

// Callback specification for `SetSigninChoiceCallback()`.
using IntroSigninChoiceCallback =
    base::StrongAlias<class IntroSigninChoiceCallbackTag,
                      base::OnceCallback<void(IntroChoice)>>;

using DefaultBrowserCallback =
    base::StrongAlias<class DefaultBrowserCallbackTag,
                      base::OnceCallback<void(DefaultBrowserChoice)>>;

class IntroUIConfig : public content::DefaultWebUIConfig<IntroUI> {
 public:
  IntroUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIIntroHost) {}
};

// The WebUI controller for `chrome://intro`.
// Drops user inputs until a callback to receive the next one is provided by
// calling `SetSigninChoiceCallback()`.
class IntroUI : public ui::MojoWebUIController,
                public intro::mojom::SignInCelebrationPageHandlerFactory,
                public intro::mojom::IntroPageHandlerFactory,
                public intro::mojom::SignInPromoPageHandlerFactory,
                public intro::mojom::FinishOrContinuePageHandlerFactory {
 public:
  explicit IntroUI(content::WebUI* web_ui);

  IntroUI(const IntroUI&) = delete;
  IntroUI& operator=(const IntroUI&) = delete;

  ~IntroUI() override;

  void SetSigninChoiceCallback(IntroSigninChoiceCallback callback);
  void SetDefaultBrowserCallback(DefaultBrowserCallback callback);
  void SetCanPinToTaskbar(bool can_pin);

  // Prepares the information to be given to the handler once ready.
  void SetSignInCelebrationFinishedCallback(
      base::OnceClosure celebration_finished_callback);

  void SetFinishOrContinueCallback(
      base::OnceCallback<void(FinishOrContinueChoice)> callback);

  void BindInterface(
      mojo::PendingReceiver<intro::mojom::SignInCelebrationPageHandlerFactory>
          receiver);
  void BindInterface(
      mojo::PendingReceiver<intro::mojom::IntroPageHandlerFactory> receiver);
  void BindInterface(
      mojo::PendingReceiver<intro::mojom::SignInPromoPageHandlerFactory>
          receiver);
  void BindInterface(
      mojo::PendingReceiver<intro::mojom::FinishOrContinuePageHandlerFactory>
          receiver);

  // Called by the browser to toggle animations in the WebUI.
  void ToggleAnimations(bool active);

  // intro::mojom::SignInCelebrationPageHandlerFactory:
  void CreateSignInCelebrationPageHandler(
      mojo::PendingRemote<intro::mojom::SignInCelebrationPage> page,
      mojo::PendingReceiver<intro::mojom::SignInCelebrationPageHandler>
          receiver) override;

  // intro::mojom::IntroPageHandlerFactory:
  void CreateIntroPageHandler(
      mojo::PendingRemote<intro::mojom::IntroPage> page) override;
  // intro::mojom::SignInPromoPageHandlerFactory:
  void CreateSignInPromoPageHandler(
      mojo::PendingRemote<intro::mojom::SignInPromoPage> page,
      mojo::PendingReceiver<intro::mojom::SignInPromoPageHandler> receiver)
      override;

  // intro::mojom::FinishOrContinuePageHandlerFactory:
  void CreateFinishOrContinuePageHandler(
      mojo::PendingReceiver<intro::mojom::FinishOrContinuePageHandler> receiver)
      override;

 private:
  void HandleSigninChoice(IntroChoice choice);
  void HandleDefaultBrowserChoice(DefaultBrowserChoice choice);

  // Callback awaiting `CreatePageHandler` to create the handler with the closure.
  void OnSignInCelebrationMojoHandlerReady(
      base::OnceClosure celebration_finished_callback,
      mojo::PendingRemote<intro::mojom::SignInCelebrationPage> page,
      mojo::PendingReceiver<intro::mojom::SignInCelebrationPageHandler>
          receiver);

  void OnFinishOrContinueChoice(FinishOrContinueChoice choice);

  IntroSigninChoiceCallback signin_choice_callback_;
  DefaultBrowserCallback default_browser_callback_;
  raw_ptr<IntroHandler> intro_handler_;

  // Callback that temporarily holds the information to be passed onto the
  // handler. The callback is called once the mojo handlers are available.
  base::OnceCallback<void(
      mojo::PendingRemote<intro::mojom::SignInCelebrationPage>,
      mojo::PendingReceiver<intro::mojom::SignInCelebrationPageHandler>)>
      initialize_handler_callback_;
  std::unique_ptr<SignInCelebrationHandler>
      intro_sign_in_celebration_handler_;
  std::unique_ptr<SignInPromoHandler> sign_in_promo_handler_;

  std::unique_ptr<FinishOrContinueHandler> finish_or_continue_handler_;

  base::OnceCallback<void(FinishOrContinueChoice)> finish_or_continue_callback_;

  mojo::Receiver<intro::mojom::SignInCelebrationPageHandlerFactory>
      sign_in_celebration_factory_receiver_{this};
  mojo::Receiver<intro::mojom::SignInPromoPageHandlerFactory>
      sign_in_promo_factory_receiver_{this};

  mojo::Receiver<intro::mojom::IntroPageHandlerFactory> intro_factory_receiver_{
      this};
  mojo::Remote<intro::mojom::IntroPage> intro_page_;
  std::optional<bool> animations_active_;

  mojo::Receiver<intro::mojom::FinishOrContinuePageHandlerFactory>
      finish_or_continue_factory_receiver_{this};

  base::WeakPtrFactory<IntroUI> weak_ptr_factory_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTRO_INTRO_UI_H_
