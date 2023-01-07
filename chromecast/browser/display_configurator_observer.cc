// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/display_configurator_observer.h"

#include "chromecast/graphics/cast_window_manager_aura.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/compositor.h"

namespace chromecast {

DisplayConfiguratorObserver::DisplayConfiguratorObserver(
    chromecast::shell::CastDisplayConfigurator* display_configurator,
    chromecast::CastWindowManagerAura* manager)
    : display_configurator_(display_configurator), window_manager_(manager) {
  display_configurator_->AddObserver(this);
}

DisplayConfiguratorObserver::~DisplayConfiguratorObserver() {
  display_configurator_->RemoveObserver(this);
}

void DisplayConfiguratorObserver::OnDisplayStateChanged() {
  window_manager_->GetRootWindow()
      ->GetHost()
      ->compositor()
      ->ScheduleFullRedraw();
}

}  // namespace chromecast
