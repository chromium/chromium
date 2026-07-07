// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_AIM_ELIGIBILITY_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_AIM_ELIGIBILITY_PAGE_HANDLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/drive_picker_host/drive_disclaimer_controller.h"
#endif
#include "chrome/browser/ui/webui/omnibox/aim_eligibility/aim_eligibility.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class AimEligibilityService;
class PrefService;
class Profile;

// WebUI page handler for the chrome://omnibox/aim-eligibility.
class AimEligibilityPageHandler : public aim_eligibility::mojom::PageHandler {
 public:
  AimEligibilityPageHandler(
      Profile* profile,
      mojo::PendingReceiver<aim_eligibility::mojom::PageHandler> receiver,
      mojo::PendingRemote<aim_eligibility::mojom::Page> page);

  AimEligibilityPageHandler(const AimEligibilityPageHandler&) = delete;
  AimEligibilityPageHandler& operator=(const AimEligibilityPageHandler&) =
      delete;

  ~AimEligibilityPageHandler() override;

  // aim_eligibility::mojom::PageHandler:
  void GetEligibilityState(GetEligibilityStateCallback callback) override;
  void RequestServerEligibilityForDebugging() override;
  void SetEligibilityResponseForDebugging(
      const std::string& base64_encoded_response,
      SetEligibilityResponseForDebuggingCallback callback) override;

  void set_disconnect_handler(base::OnceClosure callback) {
    receiver_.set_disconnect_handler(std::move(callback));
  }

 private:
  // Called when the eligibility state changes.
  void OnEligibilityChanged();
#if !BUILDFLAG(IS_ANDROID)
  // Called when the Drive disclaimer status is checked.
  void OnDisclaimerStatusChecked(
      drive_picker::DriveDisclaimerController::DisclaimerStatus status);
#endif
  // Returns the current eligibility state from `AimEligibilityService`.
  aim_eligibility::mojom::EligibilityStatePtr QueryEligibilityState();
  // Returns the current Drive status.
  aim_eligibility::mojom::DriveStatusPtr QueryDriveStatus(
      aim_eligibility::mojom::DisclaimerState disclaimer_state);

  const raw_ptr<Profile> profile_;
  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<AimEligibilityService> aim_eligibility_service_;
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<drive_picker::DriveDisclaimerController>
      drive_disclaimer_controller_;
  bool disclaimer_check_started_ = false;
#endif

  mojo::Receiver<aim_eligibility::mojom::PageHandler> receiver_;
  const mojo::Remote<aim_eligibility::mojom::Page> page_;
  // Subscription to `AimEligibilityService` eligibility changed callbacks.
  base::CallbackListSubscription eligibility_changed_subscription_;

  base::WeakPtrFactory<AimEligibilityPageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_AIM_ELIGIBILITY_AIM_ELIGIBILITY_PAGE_HANDLER_H_
