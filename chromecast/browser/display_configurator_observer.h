// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_DISPLAY_CONFIGURATOR_OBSERVER_H_
#define CHROMECAST_BROWSER_DISPLAY_CONFIGURATOR_OBSERVER_H_

#include "chromecast/browser/cast_display_configurator.h"

namespace chromecast {

class CastWindowManagerAura;

// Observer class that can respond to Display Configurator state changes.
// Forces a repaint to ensure content is refreshed post display configuration
// change.
class DisplayConfiguratorObserver
    : public shell::CastDisplayConfigurator::Observer {
 public:
  DisplayConfiguratorObserver(
      shell::CastDisplayConfigurator* display_configurator,
      CastWindowManagerAura* manager);

  ~DisplayConfiguratorObserver() override;

  DisplayConfiguratorObserver(const DisplayConfiguratorObserver&) = delete;

  DisplayConfiguratorObserver& operator=(const DisplayConfiguratorObserver&) =
      delete;

  // CastDisplayConfigurator::Observer
  void OnDisplayStateChanged() override;

 private:
  shell::CastDisplayConfigurator* display_configurator_;
  CastWindowManagerAura* window_manager_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_DISPLAY_CONFIGURATOR_OBSERVER_H_
