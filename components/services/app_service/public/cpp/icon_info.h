// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_INFO_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_INFO_H_

#include <optional>

#include "base/values.h"
#include "url/gurl.h"

namespace apps {

// This struct holds info about an icon, but not the actual SkBitmap. It's
// roughly a flattened version of `blink::mojom::ManifestImageResource`, as
// there is one instance for each combination of URL, size and purpose, rather
// than one instance per URL that holds vectors of sizes and purposes.
struct IconInfo {
  using SquareSizePx = int;

  // Equivalent to `blink::mojom::ManifestImageResource_Purpose`.
  enum class Purpose {
    kAny = 0,
    kMonochrome = 1,
    kMaskable = 2,
  };

  IconInfo();
  IconInfo(const GURL& url, SquareSizePx size);
  IconInfo(const IconInfo&);
  IconInfo(IconInfo&&) noexcept;
  ~IconInfo();
  IconInfo& operator=(const IconInfo&);
  IconInfo& operator=(IconInfo&&) noexcept;
  base::Value AsDebugValue() const;

  bool operator==(const apps::IconInfo& other) const;

  // The source URL of the icon. Note that multiple icons can share a single
  // source URL, as some image formats can contain multiple sizes.
  GURL url;

  // The nominal size of the icon. Depending on context, this might have come
  // from the manifest or be a record of the size icon that was served at `url`.
  std::optional<SquareSizePx> square_size_px;

  // The purpose of this icon.
  Purpose purpose = Purpose::kAny;
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_ICON_INFO_H_
