// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOASTS_TOAST_CONTROLLER_H_
#define CHROME_BROWSER_UI_TOASTS_TOAST_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"

class BrowserWindowInterface;
class ToastRegistry;
enum class ToastId;

struct ToastParams {
  explicit ToastParams(ToastId id);
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

  bool CanShowToast(ToastId id);
  void ShowToast(ToastParams params);
  void ClosePersistentToast(ToastId id);
  bool IsShowingToast() const;

 private:
  bool is_showing_toast_ = false;

  const raw_ptr<BrowserWindowInterface> browser_window_interface_;
  const raw_ptr<const ToastRegistry> toast_registry_;
};

#endif  // CHROME_BROWSER_UI_TOASTS_TOAST_CONTROLLER_H_
