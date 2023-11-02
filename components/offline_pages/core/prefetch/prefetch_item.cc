// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_item.h"

#include <ostream>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/offline_pages/core/offline_store_utils.h"

namespace offline_pages {

PrefetchItem::PrefetchItem() = default;

PrefetchItem::PrefetchItem(PrefetchItem&& other) = default;

PrefetchItem::~PrefetchItem() = default;

}  // namespace offline_pages
