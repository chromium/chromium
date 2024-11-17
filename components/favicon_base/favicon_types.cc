// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon_base/favicon_types.h"

#include "components/favicon_base/fallback_icon_style.h"

namespace favicon_base {

// ---------------------------------------------------------
// FaviconImageResult

FaviconImageResult::FaviconImageResult() = default;

FaviconImageResult::~FaviconImageResult() = default;

// --------------------------------------------------------
// FaviconRawBitmapResult

FaviconRawBitmapResult::FaviconRawBitmapResult()
    : expired(false), icon_type(IconType::kInvalid) {}

FaviconRawBitmapResult::FaviconRawBitmapResult(
    const FaviconRawBitmapResult& other) = default;

FaviconRawBitmapResult::~FaviconRawBitmapResult() = default;

// --------------------------------------------------------
// LargeIconResult

LargeIconResult::LargeIconResult(const FaviconRawBitmapResult& bitmap_in)
    : bitmap(bitmap_in) {}

LargeIconResult::LargeIconResult(FallbackIconStyle* fallback_icon_style_in)
    : fallback_icon_style(fallback_icon_style_in) {}

LargeIconResult::~LargeIconResult() = default;

LargeIconResult::LargeIconResult(LargeIconResult&& result) = default;

// --------------------------------------------------------
// LargeIconImageResult

LargeIconImageResult::LargeIconImageResult(const gfx::Image& image_in,
                                           const GURL& icon_url_in)
    : image(image_in), icon_url(icon_url_in) {}

LargeIconImageResult::LargeIconImageResult(
    FallbackIconStyle* fallback_icon_style_in)
    : fallback_icon_style(fallback_icon_style_in) {}

LargeIconImageResult::~LargeIconImageResult() = default;

}  // namespace favicon_base
