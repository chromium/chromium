// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/hash_util.h"

#include "base/base64.h"
#include "base/hash/sha1.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/sync/base/model_type.h"
#include "components/sync/protocol/autofill_offer_specifics.pb.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"

namespace syncer {

std::string GenerateSyncableBookmarkHash(
    const std::string& originator_cache_guid,
    const std::string& originator_client_item_id) {
  // Blank PB with just the field in it has termination symbol,
  // handy for delimiter.
  sync_pb::EntitySpecifics serialized_type;
  AddDefaultFieldValue(BOOKMARKS, &serialized_type);
  std::string hash_input;
  serialized_type.AppendToString(&hash_input);
  hash_input.append(originator_cache_guid + originator_client_item_id);

  std::string encode_output;
  base::Base64Encode(base::SHA1HashString(hash_input), &encode_output);
  return encode_output;
}

std::string GetUnhashedClientTagFromAutofillWalletSpecifics(
    const sync_pb::AutofillWalletSpecifics& specifics) {
  switch (specifics.type()) {
    case sync_pb::AutofillWalletSpecifics::MASKED_CREDIT_CARD:
      return specifics.masked_card().id();
    case sync_pb::AutofillWalletSpecifics::POSTAL_ADDRESS:
      return specifics.address().id();
    case sync_pb::AutofillWalletSpecifics::CUSTOMER_DATA:
      return specifics.customer_data().id();
    case sync_pb::AutofillWalletSpecifics::CREDIT_CARD_CLOUD_TOKEN_DATA:
      return specifics.cloud_token_data().instrument_token();
    case sync_pb::AutofillWalletSpecifics::PAYMENT_INSTRUMENT:
      // Append a string to the instrument ID, since the same ID may be used for
      // a MASKED_CREDIT_CARD entry.
      return base::StrCat(
          {"payment_instrument:",
           base::NumberToString(
               specifics.payment_instrument().instrument_id())});
    case sync_pb::AutofillWalletSpecifics::UNKNOWN:
      NOTREACHED();
      return std::string();
  }
  return std::string();
}

std::string GetUnhashedClientTagFromAutofillOfferSpecifics(
    const sync_pb::AutofillOfferSpecifics& specifics) {
  return base::NumberToString(specifics.id());
}

}  // namespace syncer
