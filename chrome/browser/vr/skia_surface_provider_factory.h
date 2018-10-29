// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SKIA_SURFACE_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_VR_SKIA_SURFACE_PROVIDER_FACTORY_H_

#include <memory>

#include "chrome/browser/vr/skia_surface_provider.h"
#include "chrome/browser/vr/vr_ui_export.h"

namespace vr {

class VR_UI_EXPORT SkiaSurfaceProviderFactory {
 public:
  static std::unique_ptr<SkiaSurfaceProvider> Create();
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SKIA_SURFACE_PROVIDER_FACTORY_H_
