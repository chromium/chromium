// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOASTS_TOAST_CONTROLLER_H_
#define CHROME_BROWSER_UI_TOASTS_TOAST_CONTROLLER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"

class BrowserWindowInterface;
class ToastRegistry;
enum class ToastId;

struct ToastParams {
  explicit ToastParams(ToastId id);
  ToastParams(ToastParams&& other) noexcept;
  ToastParams& operator=(ToastParams&& other) noexcept;
  ~ToastParams();

  ToastId toast_id_;
  std::vector<std::u16string> body_string_replacement_params_;
  std::vector<std::u16string> action_button_string_replacement_params_;
};

class ToastController {
 public:
  explicit ToastController(BrowserWindowInterface* browser_window_interface,
                           const ToastRegistry* toast_registry);
  ~ToastController();

  bool IsShowingToast() const;
  bool CanShowToast(ToastId id) const;

  // Attempts to show the toast and returns true if the toast was successfully
  // shown, otherwise return false. Callers that show a persistent toast must
  // eventually call ClosePersistentToast() to ensure their toast closes.
  bool MaybeShowToast(ToastParams params);

  // Closes the currently showing persistent toast that must correspond to `id`.
  void ClosePersistentToast(ToastId id);

 private:
  void CloseToast();

  const raw_ptr<BrowserWindowInterface> browser_window_interface_;
  const raw_ptr<const ToastRegistry> toast_registry_;
  std::optional<ToastParams> current_toast_params_;
  base::OneShotTimer toast_close_timer_;
};

#endif  // CHROME_BROWSER_UI_TOASTS_TOAST_CONTROLLER_H_
