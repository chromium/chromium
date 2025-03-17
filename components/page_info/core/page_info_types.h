// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_INFO_CORE_PAGE_INFO_TYPES_H_
#define COMPONENTS_PAGE_INFO_CORE_PAGE_INFO_TYPES_H_

#include "base/functional/callback.h"
#include "url/gurl.h"

namespace page_info {

// Information returned by the merchant info APIs.
struct MerchantData {
  MerchantData();
  MerchantData(const MerchantData&);
  MerchantData& operator=(const MerchantData&);
  MerchantData(MerchantData&&);
  MerchantData& operator=(MerchantData&&);
  ~MerchantData();

  float star_rating = 0;
  uint32_t count_rating = 0;
  GURL page_url;
  std::string reviews_summary;
};

using MerchantDataCallback =
    base::OnceCallback<void(const GURL&, std::optional<MerchantData>)>;

// The open referrer of the merchant trust bubble.
enum class MerchantBubbleOpenReferrer { kPageInfo, kLocationBarChip };
}  // namespace page_info

#endif  // COMPONENTS_PAGE_INFO_CORE_PAGE_INFO_TYPES_H_
