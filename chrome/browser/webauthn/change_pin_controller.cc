// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/change_pin_controller.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/change_pin_controller_impl.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/fido/features.h"

ChangePinController* ChangePinController::instance_for_testing_ = nullptr;

ChangePinController::~ChangePinController() = default;

// static
ChangePinController* ChangePinController::ForWebContents(
    content::WebContents* web_contents) {
  if (instance_for_testing_) {
    return instance_for_testing_;
  }
  return ChangePinControllerImpl::GetOrCreateForCurrentDocument(
      web_contents->GetPrimaryMainFrame());
}
