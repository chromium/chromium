// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_MOJOM_VISIBILITY_MOJOM_TRAITS_H_
#define CHROMECAST_BROWSER_MOJOM_VISIBILITY_MOJOM_TRAITS_H_

#include "chromecast/browser/mojom/cast_content_window.mojom.h"
#include "chromecast/browser/visibility_types.h"
#include "mojo/public/cpp/bindings/enum_traits.h"

#define CASE_TRANSLATE_ENUM(x) \
  case InputType::x:           \
    return OutputType::x;

#define CASE_TRANSLATE_MOJOM_ENUM(x) \
  case InputType::x:                 \
    *out = OutputType::x;            \
    return true;

namespace mojo {

template <>
struct EnumTraits<chromecast::mojom::VisibilityType,
                  chromecast::VisibilityType> {
  static chromecast::mojom::VisibilityType ToMojom(
      chromecast::VisibilityType type) {
    using InputType = chromecast::VisibilityType;
    using OutputType = chromecast::mojom::VisibilityType;
    switch (type) {
      CASE_TRANSLATE_ENUM(UNKNOWN);
      CASE_TRANSLATE_ENUM(FULL_SCREEN);
      CASE_TRANSLATE_ENUM(PARTIAL_OUT);
      CASE_TRANSLATE_ENUM(HIDDEN);
      CASE_TRANSLATE_ENUM(TILE);
      CASE_TRANSLATE_ENUM(TRANSIENTLY_HIDDEN);
    }
    NOTREACHED();
  }

  static bool FromMojom(chromecast::mojom::VisibilityType type,
                        chromecast::VisibilityType* out) {
    using InputType = chromecast::mojom::VisibilityType;
    using OutputType = chromecast::VisibilityType;
    switch (type) {
      CASE_TRANSLATE_MOJOM_ENUM(UNKNOWN);
      CASE_TRANSLATE_MOJOM_ENUM(FULL_SCREEN);
      CASE_TRANSLATE_MOJOM_ENUM(PARTIAL_OUT);
      CASE_TRANSLATE_MOJOM_ENUM(HIDDEN);
      CASE_TRANSLATE_MOJOM_ENUM(TILE);
      CASE_TRANSLATE_MOJOM_ENUM(TRANSIENTLY_HIDDEN);
    }
    NOTREACHED();
  }
};

template <>
struct EnumTraits<chromecast::mojom::VisibilityPriority,
                  chromecast::VisibilityPriority> {
  static chromecast::mojom::VisibilityPriority ToMojom(
      chromecast::VisibilityPriority priority) {
    using InputType = chromecast::VisibilityPriority;
    using OutputType = chromecast::mojom::VisibilityPriority;
    switch (priority) {
      CASE_TRANSLATE_ENUM(DEFAULT);
      CASE_TRANSLATE_ENUM(TRANSIENT_TIMEOUTABLE);
      CASE_TRANSLATE_ENUM(HIGH_PRIORITY_INTERRUPTION);
      CASE_TRANSLATE_ENUM(STICKY_ACTIVITY);
      CASE_TRANSLATE_ENUM(TRANSIENT_STICKY);
      CASE_TRANSLATE_ENUM(HIDDEN);
      CASE_TRANSLATE_ENUM(HIDDEN_STICKY);
    }
    NOTREACHED();
  }

  static bool FromMojom(chromecast::mojom::VisibilityPriority priority,
                        chromecast::VisibilityPriority* out) {
    using InputType = chromecast::mojom::VisibilityPriority;
    using OutputType = chromecast::VisibilityPriority;
    switch (priority) {
      case chromecast::mojom::VisibilityPriority::DESTROYED:
        LOG(WARNING) << "Cannot convert mojom::VisibilityPriority::DESTROYED";
        *out = chromecast::VisibilityPriority::HIDDEN;
        return true;
        CASE_TRANSLATE_MOJOM_ENUM(DEFAULT);
        CASE_TRANSLATE_MOJOM_ENUM(TRANSIENT_TIMEOUTABLE);
        CASE_TRANSLATE_MOJOM_ENUM(HIGH_PRIORITY_INTERRUPTION);
        CASE_TRANSLATE_MOJOM_ENUM(STICKY_ACTIVITY);
        CASE_TRANSLATE_MOJOM_ENUM(TRANSIENT_STICKY);
        CASE_TRANSLATE_MOJOM_ENUM(HIDDEN);
        CASE_TRANSLATE_MOJOM_ENUM(HIDDEN_STICKY);
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // CHROMECAST_BROWSER_MOJOM_VISIBILITY_MOJOM_TRAITS_H_
