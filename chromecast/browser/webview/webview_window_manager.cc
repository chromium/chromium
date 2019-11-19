// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/webview_window_manager.h"

#include "chromecast/graphics/cast_window_manager.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "ui/aura/env.h"

namespace chromecast {

// Keeps track of the creation and destruction of webview container windows, and
// adds and removes the root window rounded corner decoration accordingly.
// Rounded corners only need to be present when webviews are being displayed.
class RoundedCornersObserver : public WebviewWindowManager::Observer,
                               public aura::WindowObserver {
 public:
  explicit RoundedCornersObserver(CastWindowManager* cast_window_manager)
      : cast_window_manager_(cast_window_manager) {}

  ~RoundedCornersObserver() override {}

  // WebviewWindowManager::Observer implementation
  void OnNewWebviewContainerWindow(aura::Window* window, int app_id) override {
    // Observe the lifecycle of this window so we can remove rounded corners
    // when it goes away.
    window->AddObserver(this);

    // Add rounded corners on the first created window.
    if (cast_window_manager_ && !num_container_windows_) {
      cast_window_manager_->SetEnableRoundedCorners(true);
    }
    num_container_windows_++;
  }

  // aura::WindowObserver implementation
  void OnWindowDestroyed(aura::Window* window) override {
    // Remove the rounded corners when we're out of container windows.
    num_container_windows_--;
    DCHECK_GE(num_container_windows_, 0);
    if (cast_window_manager_ && !num_container_windows_) {
      cast_window_manager_->SetEnableRoundedCorners(false);
    }
  }

 private:
  CastWindowManager* cast_window_manager_;
  int num_container_windows_ = 0;

  DISALLOW_COPY_AND_ASSIGN(RoundedCornersObserver);
};

WebviewWindowManager::WebviewWindowManager(
    CastWindowManager* cast_window_manager)
    : rounded_corners_observer_(
          std::make_unique<RoundedCornersObserver>(cast_window_manager)) {
  aura::Env::GetInstance()->AddObserver(this);

  AddObserver(rounded_corners_observer_.get());
}

WebviewWindowManager::~WebviewWindowManager() {
  aura::Env::GetInstance()->RemoveObserver(this);
}

void WebviewWindowManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void WebviewWindowManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void WebviewWindowManager::OnWindowInitialized(aura::Window* window) {
  observed_windows_.push_back(window);
  window->AddObserver(this);
}

void WebviewWindowManager::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
  auto it =
      std::find(observed_windows_.begin(), observed_windows_.end(), window);
  DCHECK(it != observed_windows_.end());
  observed_windows_.erase(it);
}

void WebviewWindowManager::OnWindowPropertyChanged(aura::Window* window,
                                                   const void* key,
                                                   intptr_t old) {
  if (key != exo::kClientSurfaceIdKey)
    return;

  int app_id = window->GetProperty(exo::kClientSurfaceIdKey);
  LOG(INFO) << "Found window for webview " << app_id;
  for (auto& observer : observers_)
    observer.OnNewWebviewContainerWindow(window, app_id);
}

}  // namespace chromecast
