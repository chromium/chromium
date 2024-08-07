// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_SYNC_UTIL_H_
#define COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_SYNC_UTIL_H_

#include "components/plus_addresses/plus_address_types.h"
#include "components/sync/protocol/entity_data.h"

namespace plus_addresses {
// Utils to convert a `EntityData` containing `PlusAddressSpecifics` to a
// `PlusProfile` and back.
// Since the PLUS_ADDRESS data type is read-only on the client, it is not
// necessary to convert a `PlusProfile` to `EntityData` to upload to sync. But
// it is needed to show the stored data in sync-internals.
PlusProfile PlusProfileFromEntityData(const syncer::EntityData& entity_data);
syncer::EntityData EntityDataFromPlusProfile(const PlusProfile& profile);

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_SYNC_UTIL_H_
