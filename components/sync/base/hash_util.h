// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_HASH_UTIL_H_
#define COMPONENTS_SYNC_BASE_HASH_UTIL_H_

#include <string>

namespace sync_pb {
class AutofillOfferSpecifics;
class AutofillWalletSpecifics;
}  // namespace sync_pb

// TODO(crbug.com/41412176): Rename this file to data_type_util.h or something
// else that better reflects GetUnhashedClientTagFromAutofillWalletSpecifics()
// has nothing to do with hashes.

namespace syncer {

// A helper for extracting client tag out of the specifics for wallet data (as
// client tags don't get populated by the server). This is required in more than
// one place, so we define the algorithm here to make sure the implementation is
// consistent.
std::string GetUnhashedClientTagFromAutofillWalletSpecifics(
    const sync_pb::AutofillWalletSpecifics& specifics);

// Helper function to extract client tag from the specifics. For offer data,
// every time it is synced, it will be a full sync and this client tag is not
// populated by server.
std::string GetUnhashedClientTagFromAutofillOfferSpecifics(
    const sync_pb::AutofillOfferSpecifics& specifics);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_HASH_UTIL_H_
