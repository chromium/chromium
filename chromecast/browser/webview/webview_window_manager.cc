// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/webview_window_manager.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "components/exo/shell_surface_util.h"
#include "components/exo/surface.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/env.h"

namespace chromecast {

WebviewWindowManager::WebviewWindowManager() {
  aura::Env::GetInstance()->AddObserver(this);
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

  // Note: The property was originally an integer, and was switched to be a
  // string. For compatibility integer values are converted to a string via
  // base::NumberToString before being set as the property value.
  std::string* app_id_str = window->GetProperty(exo::kClientSurfaceIdKey);
  if (!app_id_str)
    return;

  int app_id = 0;
  if (!base::StringToInt(*app_id_str, &app_id))
    return;

  LOG(INFO) << "Found window for webview " << app_id;
  for (auto& observer : observers_)
    observer.OnNewWebviewContainerWindow(window, app_id);
}

}  // namespace chromecast
