// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOASTS_TOAST_CONTROLLER_H_
#define CHROME_BROWSER_UI_TOASTS_TOAST_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"

class BrowserView;
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
  explicit ToastController(BrowserView* browser_view);
  ~ToastController();

  bool CanShowToast(ToastId id);
  void ShowToast(ToastParams params);
  void ClosePersistentToast(ToastId id);

 private:
  const raw_ptr<BrowserView> browser_view_;
};

#endif  // CHROME_BROWSER_UI_TOASTS_TOAST_CONTROLLER_H_
