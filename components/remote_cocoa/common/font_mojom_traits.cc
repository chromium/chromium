// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/remote_cocoa/common/font_mojom_traits.h"

namespace mojo {

remote_cocoa::mojom::FontNamePtr
StructTraits<remote_cocoa::mojom::FontDataView, gfx::Font>::name(
    const gfx::Font& font) {
  auto* platform_font =
      static_cast<gfx::PlatformFontMac*>(font.platform_font());
  if (auto system_font = platform_font->GetSystemFontType();
      system_font.has_value()) {
    return remote_cocoa::mojom::FontName::NewSystemFont(*system_font);
  }
  return remote_cocoa::mojom::FontName::NewRegularFont(
      font.GetActualFontName());
}

bool StructTraits<remote_cocoa::mojom::FontDataView, gfx::Font>::Read(
    remote_cocoa::mojom::FontDataView data,
    gfx::Font* out) {
  gfx::Font::Weight weight;
  if (!data.ReadWeight(&weight)) {
    return false;
  }
  remote_cocoa::mojom::FontNameDataView name;
  data.GetNameDataView(&name);
  switch (name.tag()) {
    case remote_cocoa::mojom::FontNameDataView::Tag::kSystemFont:
      gfx::PlatformFontMac::SystemFontType font_type;
      if (!name.ReadSystemFont(&font_type)) {
        return false;
      }
      *out = gfx::Font(new gfx::PlatformFontMac(font_type));
      *out =
          out->Derive(data.size() - out->GetFontSize(), data.style(), weight);
      return true;
    case remote_cocoa::mojom::FontNameDataView::Tag::kRegularFont:
      std::string font_name;
      if (!name.ReadRegularFont(&font_name)) {
        return false;
      }
      *out = gfx::Font(font_name, data.size()).Derive(0, data.style(), weight);
      return true;
  }
}

}  // namespace mojo
