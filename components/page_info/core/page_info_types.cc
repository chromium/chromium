// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_info/core/page_info_types.h"

namespace page_info {
  MerchantData::MerchantData() = default;
  MerchantData::MerchantData(const MerchantData&) = default;
  MerchantData& MerchantData::operator=(const MerchantData&) = default;
  MerchantData& MerchantData::operator=(MerchantData&&) = default;
  MerchantData::MerchantData(MerchantData&&) = default;
  MerchantData::~MerchantData() = default;
}  // namespace page_info
