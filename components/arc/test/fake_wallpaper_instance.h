// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_WALLPAPER_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_WALLPAPER_INSTANCE_H_

#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "components/arc/mojom/wallpaper.mojom.h"

namespace arc {

class FakeWallpaperInstance : public mojom::WallpaperInstance {
 public:
  FakeWallpaperInstance();
  ~FakeWallpaperInstance() override;

  const std::vector<int32_t>& changed_ids() const { return changed_ids_; }

  // Overridden from mojom::WallpaperInstance
  void InitDeprecated(mojom::WallpaperHostPtr host_ptr) override;
  void Init(mojom::WallpaperHostPtr host_ptr, InitCallback callback) override;
  void OnWallpaperChanged(int32_t walpaper_id) override;

 private:
  std::vector<int32_t> changed_ids_;

  // Keeps the binding alive so that calls to this class can be correctly
  // routed.
  mojom::WallpaperHostPtr host_;

  DISALLOW_COPY_AND_ASSIGN(FakeWallpaperInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_WALLPAPER_INSTANCE_H_
