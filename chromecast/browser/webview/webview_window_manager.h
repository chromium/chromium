// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_WINDOW_MANAGER_H_
#define CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_WINDOW_MANAGER_H_

#include "base/observer_list.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"

namespace chromecast {

class CastWindowManager;
class RoundedCornersObserver;

// Keeps track of new aura::Windows and listen for window property events to
// find Exo windows with the |exo::kClientSurfaceIdKey| property set.
class WebviewWindowManager : public aura::EnvObserver,
                             public aura::WindowObserver {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    // Notifies the observer when a window's |exo::kClientSurfaceIdKey| property
    // is updated.
    virtual void OnNewWebviewContainerWindow(aura::Window* window,
                                             int app_id) = 0;
  };

  explicit WebviewWindowManager(CastWindowManager* cast_window_manager);
  ~WebviewWindowManager() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  void OnWindowInitialized(aura::Window* window) override;

  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  std::vector<aura::Window*> observed_windows_;

  base::ObserverList<Observer>::Unchecked observers_;
  std::unique_ptr<RoundedCornersObserver> rounded_corners_observer_;

  DISALLOW_COPY_AND_ASSIGN(WebviewWindowManager);
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBVIEW_WEBVIEW_WINDOW_MANAGER_H_
