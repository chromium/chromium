// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_UI_AURA_COMPONENTS_IMPL_H_
#define CHROMECAST_UI_AURA_COMPONENTS_IMPL_H_

#include <memory>

#include "chromecast/ui/aura_components.h"
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
class AuraComponentsImpl : public AuraComponents {
 public:
  explicit AuraComponentsImpl(CastWindowManager* cast_window_manager);
  ~AuraComponentsImpl() override;

 private:
  std::unique_ptr<views::LayoutProvider> layout_provider_;
};

}  // namespace chromecast

#endif  // CHROMECAST_UI_AURA_COMPONENTS_IMPL_H_
