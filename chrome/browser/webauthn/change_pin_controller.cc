// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/change_pin_controller.h"

#include "chrome/browser/webauthn/change_pin_controller_impl.h"
#include "content/public/browser/web_contents.h"

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
