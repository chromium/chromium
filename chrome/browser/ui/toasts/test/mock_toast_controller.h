// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOASTS_TEST_MOCK_TOAST_CONTROLLER_H_
#define CHROME_BROWSER_UI_TOASTS_TEST_MOCK_TOAST_CONTROLLER_H_

#include "chrome/browser/ui/toasts/toast_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

class BrowserWindowInterface;
class ToastRegistry;

// gMock `ToastController` subclass that mocks `MaybeShowToast()`.
class MockToastController : public ToastController {
 public:
  // Creates a mock controller backed by optional `browser_window_interface`
  // and `toast_registry` pointers.
  explicit MockToastController(
      BrowserWindowInterface* browser_window_interface = nullptr,
      const ToastRegistry* toast_registry = nullptr);
  MockToastController(const MockToastController&) = delete;
  MockToastController& operator=(const MockToastController&) = delete;
  ~MockToastController() override;

  // ToastController:
  MOCK_METHOD(bool, MaybeShowToast, (ToastParams params), (override));
};

#endif  // CHROME_BROWSER_UI_TOASTS_TEST_MOCK_TOAST_CONTROLLER_H_
