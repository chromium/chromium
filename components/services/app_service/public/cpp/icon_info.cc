// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/icon_info.h"

namespace apps {

// IconInfo
IconInfo::IconInfo() = default;
IconInfo::IconInfo(const GURL& url, SquareSizePx size)
    : url(url), square_size_px(size) {}

IconInfo::IconInfo(const IconInfo&) = default;

IconInfo::IconInfo(IconInfo&&) = default;

IconInfo::~IconInfo() = default;

IconInfo& IconInfo::operator=(const IconInfo&) = default;

IconInfo& IconInfo::operator=(IconInfo&&) = default;

base::Value IconInfo::AsDebugValue() const {
  base::Value root(base::Value::Type::DICTIONARY);
  root.SetStringKey("url", url.spec());
  root.SetKey("square_size_px",
              square_size_px ? base::Value(*square_size_px) : base::Value());
  root.SetKey("purpose", base::Value(static_cast<int>(purpose)));
  return root;
}

}  // namespace apps
