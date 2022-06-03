// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_UI_AURA_COMPONENTS_H_
#define CHROMECAST_UI_AURA_COMPONENTS_H_

#include <memory>

#include "chromecast/ui/media_overlay.h"
#include "ui/views/layout/layout_provider.h"

namespace chromecast {

class CastWindowManager;

// Collection of Cast platform objects which only have valid implementations on
// Aura platforms. All getters to this class will return nullptr on non-Aura
// platforms.
//
// This class helps avoid usage of "#if defined(USE_AURA)" macros by using pure
// virtual interfaces to wrap Aura-specific code. Clients to these interfaces
// can therefore be written in a platform-agnostic way.
class AuraComponents {
 public:
  static std::unique_ptr<AuraComponents> Create(
      CastWindowManager* cast_window_manager);

  AuraComponents();
  virtual ~AuraComponents();

  MediaOverlay* media_overlay() const { return media_overlay_.get(); }

 protected:
  std::unique_ptr<MediaOverlay> media_overlay_;
};

}  // namespace chromecast

#endif  // CHROMECAST_UI_AURA_COMPONENTS_H_
