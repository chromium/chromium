// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNOUT_CONFIRMATION_TEST_SIGNOUT_CONFIRMATION_HANDLER_WAITER_H_
#define CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNOUT_CONFIRMATION_TEST_SIGNOUT_CONFIRMATION_HANDLER_WAITER_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation_ui.h"

// Helper class to wait for the SignoutConfirmationUI handler to be ready.
class TestSignoutConfirmationHandlerWaiter
    : public SignoutConfirmationUI::Observer {
 public:
  explicit TestSignoutConfirmationHandlerWaiter(
      SignoutConfirmationUI* signout_confirmation_ui);
  ~TestSignoutConfirmationHandlerWaiter() override;

  TestSignoutConfirmationHandlerWaiter(
      const TestSignoutConfirmationHandlerWaiter&) = delete;
  TestSignoutConfirmationHandlerWaiter& operator=(
      const TestSignoutConfirmationHandlerWaiter&) = delete;

  // Waits until the handler is ready. Returns immediately if already ready.
  void Wait();

 private:
  // SignoutConfirmationUI::Observer:
  void OnSignoutConfirmationUIHandlerReady() override;
  void OnSignoutConfirmationUIDestroying(SignoutConfirmationUI* ui) override;

  base::RunLoop run_loop_;
  raw_ptr<SignoutConfirmationUI> signout_confirmation_ui_;
  base::ScopedObservation<SignoutConfirmationUI,
                          SignoutConfirmationUI::Observer>
      signout_confirmation_ui_observation_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIGNIN_SIGNOUT_CONFIRMATION_TEST_SIGNOUT_CONFIRMATION_HANDLER_WAITER_H_
