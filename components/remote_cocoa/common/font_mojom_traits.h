// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_COMMON_FONT_MOJOM_TRAITS_H_
#define COMPONENTS_REMOTE_COCOA_COMMON_FONT_MOJOM_TRAITS_H_

#include "base/numerics/safe_conversions.h"
#include "components/remote_cocoa/common/font.mojom.h"
#include "ui/gfx/font.h"
#include "ui/gfx/platform_font_mac.h"

namespace mojo {

template <>
struct EnumTraits<remote_cocoa::mojom::SystemFont,
                  gfx::PlatformFontMac::SystemFontType> {
  static remote_cocoa::mojom::SystemFont ToMojom(
      gfx::PlatformFontMac::SystemFontType input) {
    switch (input) {
      case gfx::PlatformFontMac::SystemFontType::kGeneral:
        return remote_cocoa::mojom::SystemFont::kGeneral;
      case gfx::PlatformFontMac::SystemFontType::kMenu:
        return remote_cocoa::mojom::SystemFont::kMenu;
      case gfx::PlatformFontMac::SystemFontType::kToolTip:
        return remote_cocoa::mojom::SystemFont::kToolTip;
    }
    NOTREACHED_IN_MIGRATION();
  }

  static bool FromMojom(remote_cocoa::mojom::SystemFont input,
                        gfx::PlatformFontMac::SystemFontType* out) {
    switch (input) {
      case remote_cocoa::mojom::SystemFont::kGeneral:
        *out = gfx::PlatformFontMac::SystemFontType::kGeneral;
        return true;
      case remote_cocoa::mojom::SystemFont::kMenu:
        *out = gfx::PlatformFontMac::SystemFontType::kMenu;
        return true;
      case remote_cocoa::mojom::SystemFont::kToolTip:
        *out = gfx::PlatformFontMac::SystemFontType::kToolTip;
        return true;
    }
    return false;
  }
};

template <>
struct EnumTraits<remote_cocoa::mojom::FontWeight, gfx::Font::Weight> {
  static remote_cocoa::mojom::FontWeight ToMojom(gfx::Font::Weight input) {
    return static_cast<remote_cocoa::mojom::FontWeight>(input);
  }

  static bool FromMojom(remote_cocoa::mojom::FontWeight input,
                        gfx::Font::Weight* out) {
    switch (input) {
      case remote_cocoa::mojom::FontWeight::kThin:
      case remote_cocoa::mojom::FontWeight::kExtraLight:
      case remote_cocoa::mojom::FontWeight::kLight:
      case remote_cocoa::mojom::FontWeight::kNormal:
      case remote_cocoa::mojom::FontWeight::kMedium:
      case remote_cocoa::mojom::FontWeight::kSemibold:
      case remote_cocoa::mojom::FontWeight::kBold:
      case remote_cocoa::mojom::FontWeight::kExtraBold:
      case remote_cocoa::mojom::FontWeight::kBlack:
        *out = static_cast<gfx::Font::Weight>(input);
        return true;
    }
    return false;
  }
};

static_assert(static_cast<int>(remote_cocoa::mojom::FontWeight::kThin) ==
              static_cast<int>(gfx::Font::Weight::THIN));
static_assert(static_cast<int>(remote_cocoa::mojom::FontWeight::kExtraLight) ==
              static_cast<int>(gfx::Font::Weight::EXTRA_LIGHT));
static_assert(static_cast<int>(remote_cocoa::mojom::FontWeight::kLight) ==
              static_cast<int>(gfx::Font::Weight::LIGHT));
static_assert(static_cast<int>(remote_cocoa::mojom::FontWeight::kNormal) ==
              static_cast<int>(gfx::Font::Weight::NORMAL));
static_assert(static_cast<int>(remote_cocoa::mojom::FontWeight::kMedium) ==
              static_cast<int>(gfx::Font::Weight::MEDIUM));
static_assert(static_cast<int>(remote_cocoa::mojom::FontWeight::kSemibold) ==
              static_cast<int>(gfx::Font::Weight::SEMIBOLD));
static_assert(static_cast<int>(remote_cocoa::mojom::FontWeight::kBold) ==
              static_cast<int>(gfx::Font::Weight::BOLD));
static_assert(static_cast<int>(remote_cocoa::mojom::FontWeight::kExtraBold) ==
              static_cast<int>(gfx::Font::Weight::EXTRA_BOLD));
static_assert(static_cast<int>(remote_cocoa::mojom::FontWeight::kBlack) ==
              static_cast<int>(gfx::Font::Weight::BLACK));

template <>
struct StructTraits<remote_cocoa::mojom::FontDataView, gfx::Font> {
  static remote_cocoa::mojom::FontNamePtr name(const gfx::Font& font);
  static uint32_t size(const gfx::Font& font) {
    return base::checked_cast<uint32_t>(font.GetFontSize());
  }
  static uint32_t style(const gfx::Font& font) {
    return base::checked_cast<uint32_t>(font.GetStyle());
  }
  static gfx::Font::Weight weight(const gfx::Font& font) {
    return font.GetWeight();
  }
  static bool Read(remote_cocoa::mojom::FontDataView data, gfx::Font* out);
};

}  // namespace mojo

#endif  //  COMPONENTS_REMOTE_COCOA_COMMON_FONT_MOJOM_TRAITS_H_
