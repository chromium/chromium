// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_EXPERIENCES_ARC_INTENT_HELPER_ADAPTIVE_ICON_DELEGATE_H_
#define CHROMEOS_ASH_EXPERIENCES_ARC_INTENT_HELPER_ADAPTIVE_ICON_DELEGATE_H_

#include <vector>

#include "base/functional/callback.h"
#include "chromeos/ash/experiences/arc/intent_helper/activity_icon_loader.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace arc {

class AdaptiveIconDelegate {
 public:
  using AdaptiveIconDelegateCallback = base::OnceCallback<void(
      const std::vector<gfx::ImageSkia>& adaptive_icons)>;

  virtual ~AdaptiveIconDelegate() = default;

  // Generates adaptive icons from the |icons| and calls |callback|.
  virtual void GenerateAdaptiveIcons(
      const std::vector<internal::ActivityIconLoader::ActivityIconPtr>& icons,
      AdaptiveIconDelegateCallback callback) = 0;
};

}  // namespace arc

#endif  // CHROMEOS_ASH_EXPERIENCES_ARC_INTENT_HELPER_ADAPTIVE_ICON_DELEGATE_H_
