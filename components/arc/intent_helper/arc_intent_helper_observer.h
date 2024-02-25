// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_OBSERVER_H_
#define COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_OBSERVER_H_

#include <optional>
#include <string>

#include "ash/components/arc/mojom/intent_helper.mojom-forward.h"

namespace arc {

class ArcIntentHelperBridge;

class ArcIntentHelperObserver {
 public:
  virtual ~ArcIntentHelperObserver() = default;

  // Called when intent filters are added, removed or updated.
  // A std::nullopt |package_name| indicates that intent filters were updated
  // for all packages. Otherwise, |package_name| contains the name of the
  // package whose filters were changed.
  virtual void OnIntentFiltersUpdated(
      const std::optional<std::string>& package_name) {}

  // Called when the supported links setting ("Open Supported Links" under
  // "Open by default" in ARC Settings) is changed for one or more packages.
  // |added_packages| contains packages for which the setting was enabled,
  // |removed_packages| contains packages for which the setting was disabled.
  virtual void OnArcSupportedLinksChanged(
      const std::vector<arc::mojom::SupportedLinksPackagePtr>& added_packages,
      const std::vector<arc::mojom::SupportedLinksPackagePtr>& removed_packages,
      arc::mojom::SupportedLinkChangeSource source) {}

  virtual void OnIconInvalidated(const std::string& package_name) {}

  // Called when ArcIntentHelperBridge is shut down.
  virtual void OnArcIntentHelperBridgeShutdown(ArcIntentHelperBridge* bridge) {}
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_ARC_INTENT_HELPER_OBSERVER_H_
