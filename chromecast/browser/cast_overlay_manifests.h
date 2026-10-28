// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_OVERLAY_MANIFESTS_H_
#define CHROMECAST_BROWSER_CAST_OVERLAY_MANIFESTS_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace chromecast {
namespace shell {

// Returns the manifest Cast amends to Content's content_browser service
// manifest. This allows Cast to extend the capabilities exposed and/or
// required by content_browser service instances.
const service_manager::Manifest& GetCastContentBrowserOverlayManifest();

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_OVERLAY_MANIFESTS_H_
