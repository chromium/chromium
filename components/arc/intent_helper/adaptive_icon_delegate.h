// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_ADAPTIVE_ICON_DELEGATE_H_
#define COMPONENTS_ARC_INTENT_HELPER_ADAPTIVE_ICON_DELEGATE_H_

#include <vector>

#include "base/callback.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "ui/gfx/image/image_skia.h"

namespace arc {

class AdaptiveIconDelegate {
 public:
  using AdaptiveIconDelegateCallback = base::OnceCallback<void(
      const std::vector<gfx::ImageSkia>& adaptive_icons)>;

  virtual ~AdaptiveIconDelegate() = default;

  // Generates adaptive icons from the |icons| and calls |callback|.
  virtual void GenerateAdaptiveIcons(
      const std::vector<mojom::ActivityIconPtr>& icons,
      AdaptiveIconDelegateCallback callback) = 0;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_ADAPTIVE_ICON_DELEGATE_H_
