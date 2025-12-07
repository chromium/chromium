// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOASTS_TOAST_SERVICE_H_
#define CHROME_BROWSER_UI_TOASTS_TOAST_SERVICE_H_

#include <memory>

class BrowserWindowInterface;
class ToastController;
class ToastRegistry;

// The ToastService serves as the centralized location for all features to
// register its toast.
class ToastService {
 public:
  explicit ToastService(BrowserWindowInterface* browser_window_interface);
  ~ToastService();

  const ToastRegistry* toast_registry() { return toast_registry_.get(); }

  ToastController* toast_controller() { return toast_controller_.get(); }

 private:
  void RegisterToasts(BrowserWindowInterface* browser_window_interface);
  std::unique_ptr<ToastRegistry> toast_registry_;
  std::unique_ptr<ToastController> toast_controller_;
};

#endif  // CHROME_BROWSER_UI_TOASTS_TOAST_SERVICE_H_
