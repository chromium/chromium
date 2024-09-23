// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_MOJOM_WEB_MOJOM_TRAITS_H_
#define CHROMECAST_BROWSER_MOJOM_WEB_MOJOM_TRAITS_H_

#include "chromecast/browser/mojom/cast_web_contents.mojom.h"
#include "chromecast/browser/web_types.h"
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
struct EnumTraits<chromecast::mojom::PageState, chromecast::PageState> {
  static chromecast::mojom::PageState ToMojom(chromecast::PageState state) {
    using InputType = chromecast::PageState;
    using OutputType = chromecast::mojom::PageState;
    switch (state) {
      CASE_TRANSLATE_ENUM(IDLE);
      CASE_TRANSLATE_ENUM(LOADING);
      CASE_TRANSLATE_ENUM(LOADED);
      CASE_TRANSLATE_ENUM(CLOSED);
      CASE_TRANSLATE_ENUM(DESTROYED);
      CASE_TRANSLATE_ENUM(ERROR);
    }
    NOTREACHED();
  }

  static bool FromMojom(chromecast::mojom::PageState state,
                        chromecast::PageState* out) {
    using InputType = chromecast::mojom::PageState;
    using OutputType = chromecast::PageState;
    switch (state) {
      CASE_TRANSLATE_MOJOM_ENUM(IDLE);
      CASE_TRANSLATE_MOJOM_ENUM(LOADING);
      CASE_TRANSLATE_MOJOM_ENUM(LOADED);
      CASE_TRANSLATE_MOJOM_ENUM(CLOSED);
      CASE_TRANSLATE_MOJOM_ENUM(DESTROYED);
      CASE_TRANSLATE_MOJOM_ENUM(ERROR);
    }
    NOTREACHED();
  }
};
}  // namespace mojo

#endif  // CHROMECAST_BROWSER_MOJOM_WEB_MOJOM_TRAITS_H_
