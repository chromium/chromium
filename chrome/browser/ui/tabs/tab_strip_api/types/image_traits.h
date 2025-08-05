// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TYPES_IMAGE_TRAITS_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TYPES_IMAGE_TRAITS_H_

#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api_data_model.mojom.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

using MojoImageView = tabs_api::mojom::ImageDataView;
using NativeImage = gfx::ImageSkia;

template <>
struct mojo::StructTraits<MojoImageView, NativeImage> {
  static GURL data_url(const NativeImage& native);
  static bool Read(MojoImageView view, NativeImage* out);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_API_TYPES_IMAGE_TRAITS_H_
