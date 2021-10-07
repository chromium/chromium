// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INPUT_OVERLAY_ARC_INPUT_OVERLAY_MANAGER_H_
#define COMPONENTS_ARC_INPUT_OVERLAY_ARC_INPUT_OVERLAY_MANAGER_H_

#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/strings/string_util.h"
#include "components/arc/ime/arc_ime_bridge.h"
#include "components/arc/input_overlay/touch_injector.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ime/input_method.h"

namespace content {
class BrowserContext;
}

namespace arc {

class ArcBridgeService;

// Manager for ARC input overlay feature which improves input compatibility
// for touch-only apps.
class ArcInputOverlayManager : public KeyedService,
                               public aura::EnvObserver,
                               public aura::WindowObserver {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcInputOverlayManager* GetForBrowserContext(
      content::BrowserContext* context);
  ArcInputOverlayManager(content::BrowserContext* browser_context,
                         ArcBridgeService* arc_bridge_service);
  ArcInputOverlayManager(const ArcInputOverlayManager&) = delete;
  ArcInputOverlayManager& operator=(const ArcInputOverlayManager&) = delete;
  ~ArcInputOverlayManager() override;

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* new_window) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  void OnWindowDestroying(aura::Window* window) override;

  // KeyedService overrides:
  void Shutdown() override;

 private:
  friend class ArcInputOverlayManagerTest;

  class InputMethodObserver;

  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observation_{this};
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      window_observations_{this};
  base::flat_map<aura::Window*, std::unique_ptr<TouchInjector>>
      input_overlay_enabled_windows_;
  bool is_text_input_active_ = false;
  ui::InputMethod* input_method_ = nullptr;
  std::unique_ptr<InputMethodObserver> input_method_observer_;

  void ReadData(const std::string& package_name,
                aura::Window* top_level_window);

  base::WeakPtrFactory<ArcInputOverlayManager> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INPUT_OVERLAY_ARC_INPUT_OVERLAY_MANAGER_H_
