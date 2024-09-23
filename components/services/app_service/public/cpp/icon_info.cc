// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/services/app_service/public/cpp/icon_info.h"

#include <utility>

namespace apps {

namespace {

const char* const kPurposeStrings[] = {"kAny", "kMonochrome", "kMaskable"};

}  // namespace

// IconInfo
IconInfo::IconInfo() = default;
IconInfo::IconInfo(const GURL& url, SquareSizePx size)
    : url(url), square_size_px(size) {}

IconInfo::IconInfo(const IconInfo&) = default;

IconInfo::IconInfo(IconInfo&&) noexcept = default;

IconInfo::~IconInfo() = default;

IconInfo& IconInfo::operator=(const IconInfo&) = default;

IconInfo& IconInfo::operator=(IconInfo&&) noexcept = default;

base::Value IconInfo::AsDebugValue() const {
  base::Value::Dict root;
  root.Set("url", url.spec());
  root.Set("square_size_px",
           square_size_px ? base::Value(*square_size_px) : base::Value());
  root.Set("purpose", kPurposeStrings[static_cast<int>(purpose)]);
  return base::Value(std::move(root));
}

bool IconInfo::operator==(const IconInfo& other) const {
  return std::tie(url, square_size_px, purpose) ==
         std::tie(other.url, other.square_size_px, other.purpose);
}

}  // namespace apps
