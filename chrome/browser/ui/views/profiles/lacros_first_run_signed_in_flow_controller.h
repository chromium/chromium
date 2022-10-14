// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_LACROS_FIRST_RUN_SIGNED_IN_FLOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_LACROS_FIRST_RUN_SIGNED_IN_FLOW_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/profiles/profile_management_utils.h"
#include "chrome/browser/ui/views/profiles/profile_picker_signed_in_flow_controller.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class LacrosFirstRunSignedInFlowController
    : public ProfilePickerSignedInFlowController {
 public:
  // `finish_flow_callback` will be called when the user completes the FRE, but
  // might not be executed, for example if this object is destroyed before the
  // flow is completed.
  LacrosFirstRunSignedInFlowController(
      ProfilePickerWebContentsHost* host,
      Profile* profile,
      std::unique_ptr<content::WebContents> contents,
      FinishFlowCallback finish_flow_callback);
  ~LacrosFirstRunSignedInFlowController() override;

  bool sync_confirmation_seen() const { return sync_confirmation_seen_; }

  base::WeakPtr<LacrosFirstRunSignedInFlowController> GetWeakPtr() const {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // ProfilePickerSignedInFlowController:
  void Init() override;
  void FinishAndOpenBrowser(PostHostClearedCallback callback) override;
  void SwitchToSyncConfirmation() override;

 protected:
  void PreShowScreenForDebug() override;

 private:
  // Tracks whether the user got to the last step of the FRE flow.
  bool sync_confirmation_seen_ = false;

  // Callback that will be called when the user completes all the steps in the
  // flow, to finalize and close it.
  FinishFlowCallback finish_flow_callback_;

  std::unique_ptr<signin::IdentityManager::Observer> can_retry_init_observer_;

  base::WeakPtrFactory<LacrosFirstRunSignedInFlowController> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_LACROS_FIRST_RUN_SIGNED_IN_FLOW_CONTROLLER_H_
