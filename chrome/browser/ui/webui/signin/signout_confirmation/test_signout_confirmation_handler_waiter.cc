// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signout_confirmation/test_signout_confirmation_handler_waiter.h"

#include "base/check.h"

TestSignoutConfirmationHandlerWaiter::TestSignoutConfirmationHandlerWaiter(
    SignoutConfirmationUI* signout_confirmation_ui)
    : signout_confirmation_ui_(signout_confirmation_ui) {
  CHECK(signout_confirmation_ui_);
  signout_confirmation_ui_observation_.Observe(signout_confirmation_ui_);
}

TestSignoutConfirmationHandlerWaiter::~TestSignoutConfirmationHandlerWaiter() =
    default;

void TestSignoutConfirmationHandlerWaiter::Wait() {
  if (signout_confirmation_ui_->IsHandlerReadyForTesting()) {
    return;
  }
  run_loop_.Run();
}

// SignoutConfirmationUI::Observer:
void TestSignoutConfirmationHandlerWaiter::
    OnSignoutConfirmationUIHandlerReady() {
  run_loop_.Quit();
}

void TestSignoutConfirmationHandlerWaiter::OnSignoutConfirmationUIDestroying(
    SignoutConfirmationUI* ui) {
  CHECK_EQ(ui, signout_confirmation_ui_);

  run_loop_.Quit();
  signout_confirmation_ui_observation_.Reset();
  signout_confirmation_ui_ = nullptr;
}
