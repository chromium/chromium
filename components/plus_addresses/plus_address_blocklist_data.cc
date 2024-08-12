// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_blocklist_data.h"

#include <string>

#include "base/no_destructor.h"

namespace plus_addresses {

// static
PlusAddressBlocklistData& PlusAddressBlocklistData::GetInstance() {
  static base::NoDestructor<PlusAddressBlocklistData> instance;
  return *instance;
}

PlusAddressBlocklistData::PlusAddressBlocklistData() = default;
PlusAddressBlocklistData::~PlusAddressBlocklistData() = default;

bool PlusAddressBlocklistData::PopulateDataFromComponent(
    const std::string& binary_pb) {
  // TODO(crbug.com/324556906): Parse binary data.
  return false;
}

const re2::RE2* PlusAddressBlocklistData::GetExclusionPattern() const {
  // TODO(crbug.com/324556906): Complete.
  return nullptr;
}

const re2::RE2* PlusAddressBlocklistData::GetExceptionPattern() const {
  // TODO(crbug.com/324556906): Complete.
  return nullptr;
}

}  // namespace plus_addresses
