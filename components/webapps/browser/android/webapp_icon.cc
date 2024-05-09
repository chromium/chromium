// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/webapp_icon.h"

#include "components/webapps/browser/android/webapps_icon_utils.h"

namespace webapps {

WebappIcon::WebappIcon(const GURL& icon_url) : url_(icon_url) {}

WebappIcon::WebappIcon(const GURL& icon_url,
                       bool is_maskable,
                       webapk::Image::Usage usage)
    : url_(icon_url),
      purpose_(is_maskable ? webapk::Image::MASKABLE : webapk::Image::ANY) {
  AddUsage(usage);
}

WebappIcon::~WebappIcon() = default;

void WebappIcon::AddUsage(webapk::Image::Usage usage) {
  usages_.insert(usage);
}

int WebappIcon::GetIdealSizeInPx() const {
  // The ideal size is the biggest one this icon serves.
  int ideal_size = 0;
  for (const auto& usage : usages_) {
    ideal_size = std::max(
        ideal_size,
        WebappsIconUtils::GetIdealIconSizeForIconType(usage, purpose_));
  }
  return ideal_size;
}

void WebappIcon::SetData(std::string&& data) {
  unsafe_data_ = std::move(data);
  has_unsafe_data_ = true;
}

std::string&& WebappIcon::ExtractData() {
  return std::move(unsafe_data_);
}

}  // namespace webapps
