// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_MODEL_WEB_APP_ICON_TYPES_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_MODEL_WEB_APP_ICON_TYPES_H_

#include <array>
#include <functional>
#include <map>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/values.h"
#include "components/services/app_service/public/cpp/icon_info.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace web_app {

// A map of icon urls to the bitmaps provided by that url.
using IconsMap = std::map<GURL, std::vector<SkBitmap>>;

using SquareSizePx = int;
// Iterates in ascending order (checked in SortedSizesPxIsAscending test).
using SortedSizesPx = base::flat_set<SquareSizePx, std::less<>>;
using IconPurpose = blink::mojom::ManifestImageResource_Purpose;
constexpr std::array<IconPurpose,
                     static_cast<int>(IconPurpose::kMaxValue) -
                         static_cast<int>(IconPurpose::kMinValue) + 1>
    kIconPurposes{IconPurpose::ANY, IconPurpose::MONOCHROME,
                  IconPurpose::MASKABLE};

struct SizeComparator {
  constexpr bool operator()(const gfx::Size& left,
                            const gfx::Size& right) const {
    if (left.height() != right.height()) {
      return left.height() < right.height();
    }

    return left.width() < right.width();
  }
};

using SizeSet = base::flat_set<gfx::Size, SizeComparator>;

apps::IconInfo::Purpose ManifestPurposeToIconInfoPurpose(
    IconPurpose manifest_purpose);

// Icon bitmaps for each IconPurpose.
struct IconBitmaps {
  IconBitmaps();
  IconBitmaps(const IconBitmaps&);
  IconBitmaps(IconBitmaps&&) noexcept;
  IconBitmaps& operator=(const IconBitmaps&);
  IconBitmaps& operator=(IconBitmaps&&) noexcept;
  ~IconBitmaps();

  bool operator==(const IconBitmaps&) const;

  const std::map<SquareSizePx, SkBitmap>& GetBitmapsForPurpose(
      IconPurpose purpose) const;
  void SetBitmapsForPurpose(IconPurpose purpose,
                            std::map<SquareSizePx, SkBitmap> bitmaps);

  bool empty() const;

  // TODO(crbug.com/40158740): Consider using base::flat_map.

  // Icon bitmaps suitable for any context, keyed by their square size.
  // See https://www.w3.org/TR/appmanifest/#dfn-any-purpose
  std::map<SquareSizePx, SkBitmap> any;

  // Icon bitmaps designed for masking, keyed by their square size.
  // See https://www.w3.org/TR/appmanifest/#dfn-maskable-purpose
  std::map<SquareSizePx, SkBitmap> maskable;

  // Monochrome bitmaps designed for any context, keyed by their square size.
  // See https://www.w3.org/TR/appmanifest/#purpose-member
  std::map<SquareSizePx, SkBitmap> monochrome;
};

// Icon sizes for each IconPurpose.
struct IconSizes {
  IconSizes();
  IconSizes(const IconSizes&);
  IconSizes(IconSizes&&) noexcept;
  IconSizes& operator=(const IconSizes&);
  IconSizes& operator=(IconSizes&&) noexcept;
  ~IconSizes();
  base::Value AsDebugValue() const;

  const std::vector<SquareSizePx>& GetSizesForPurpose(
      IconPurpose purpose) const;
  void SetSizesForPurpose(IconPurpose purpose, std::vector<SquareSizePx> sizes);

  bool empty() const;

  // Sizes of icon bitmaps suitable for any context.
  // See https://www.w3.org/TR/appmanifest/#dfn-any-purpose
  std::vector<SquareSizePx> any;

  // Sizes of icon bitmaps designed for masking.
  // See https://www.w3.org/TR/appmanifest/#dfn-maskable-purpose
  std::vector<SquareSizePx> maskable;

  // Sizes of monochrome bitmaps, keyed by their square size.
  // See https://www.w3.org/TR/appmanifest/#purpose-member
  std::vector<SquareSizePx> monochrome;
};

bool operator==(const IconSizes& icon_sizes1, const IconSizes& icon_sizes2);

using ShortcutsMenuIconBitmaps = std::vector<IconBitmaps>;

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_MODEL_WEB_APP_ICON_TYPES_H_
