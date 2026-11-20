// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_ICON_SOURCE_H_
#define COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_ICON_SOURCE_H_

#include <optional>

#include "base/auto_reset.h"
#include "base/observer_list_types.h"
#include "third_party/omnibox_proto/icon_resource_ids.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace omnibox {

// Renders icons looked up by IconResourceIds on request, independent of the
// surface that shows them. All methods must be called on the UI sequence.
class OmniboxIconSource {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // The set of available icons changed.
    virtual void OnIconsChanged() = 0;
  };

  virtual ~OmniboxIconSource() = default;

  // Returns the icon as a bitmap. Monochrome paths are painted in `tint_color`.
  virtual std::optional<SkBitmap> RenderBitmap(IconResourceIds id,
                                               int size_px,
                                               SkColor tint_color) = 0;

  // Returns whether the icon has its own colors, which RenderBitmap() keeps
  // regardless of `tint_color`, so callers must not re-tint it. False for
  // monochrome and unknown icons.
  virtual bool IsMultiColor(IconResourceIds id) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

// Returns the process-wide source. It currently knows no icons, so callers
// fall back to their built-in icons.
OmniboxIconSource& GetOmniboxIconSource();

// Replaces the source returned by GetOmniboxIconSource() for the lifetime
// of the returned object. `source` must outlive the returned object.
[[nodiscard]] base::AutoReset<OmniboxIconSource*>
SetOmniboxIconSourceForTesting(OmniboxIconSource* source);

}  // namespace omnibox

#endif  // COMPONENTS_OMNIBOX_BROWSER_OMNIBOX_ICON_SOURCE_H_
