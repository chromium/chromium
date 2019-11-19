// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/engine_util.h"

#include <stddef.h>

#include <memory>

#include "base/logging.h"
#include "base/stl_util.h"
#include "components/sync/nigori/cryptographer.h"
#include "components/sync/protocol/password_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync/protocol/wifi_configuration_specifics.pb.h"

namespace syncer {

namespace {

bool EndsWithSpace(const std::string& string) {
  return !string.empty() && *string.rbegin() == ' ';
}
}

std::unique_ptr<sync_pb::PasswordSpecificsData> DecryptPasswordSpecifics(
    const sync_pb::EntitySpecifics& specifics,
    const Cryptographer* crypto) {
  if (!specifics.has_password())
    return nullptr;
  const sync_pb::PasswordSpecifics& password_specifics = specifics.password();
  if (!password_specifics.has_encrypted())
    return nullptr;
  const sync_pb::EncryptedData& encrypted = password_specifics.encrypted();
  std::unique_ptr<sync_pb::PasswordSpecificsData> data =
      std::make_unique<sync_pb::PasswordSpecificsData>();
  if (!crypto->CanDecrypt(encrypted))
    return nullptr;
  if (!crypto->Decrypt(encrypted, data.get()))
    return nullptr;
  return data;
}

std::unique_ptr<sync_pb::WifiConfigurationSpecificsData>
DecryptWifiConfigurationSpecifics(const sync_pb::EntitySpecifics& specifics,
                                  const Cryptographer* crypto) {
  if (!specifics.has_wifi_configuration())
    return nullptr;
  const sync_pb::WifiConfigurationSpecifics& wifi_configuration_specifics =
      specifics.wifi_configuration();
  if (!wifi_configuration_specifics.has_encrypted())
    return nullptr;
  const sync_pb::EncryptedData& encrypted =
      wifi_configuration_specifics.encrypted();
  std::unique_ptr<sync_pb::WifiConfigurationSpecificsData> data =
      std::make_unique<sync_pb::WifiConfigurationSpecificsData>();
  if (!crypto->CanDecrypt(encrypted))
    return nullptr;
  if (!crypto->Decrypt(encrypted, data.get()))
    return nullptr;
  return data;
}

// The list of names which are reserved for use by the server.
static const char* kForbiddenServerNames[] = {"", ".", ".."};

// When taking a name from the syncapi, append a space if it matches the
// pattern of a server-illegal name followed by zero or more spaces.
void SyncAPINameToServerName(const std::string& syncer_name, std::string* out) {
  *out = syncer_name;
  if (IsNameServerIllegalAfterTrimming(*out))
    out->append(" ");
}

// In the reverse direction, if a server name matches the pattern of a
// server-illegal name followed by one or more spaces, remove the trailing
// space.
void ServerNameToSyncAPIName(const std::string& server_name, std::string* out) {
  DCHECK(out);
  int length_to_copy = server_name.length();
  if (IsNameServerIllegalAfterTrimming(server_name) &&
      EndsWithSpace(server_name)) {
    --length_to_copy;
  }
  *out = server_name.substr(0, length_to_copy);
}

// Checks whether |name| is a server-illegal name followed by zero or more space
// characters.  The three server-illegal names are the empty string, dot, and
// dot-dot.  Very long names (>255 bytes in UTF-8 Normalization Form C) are
// also illegal, but are not considered here.
bool IsNameServerIllegalAfterTrimming(const std::string& name) {
  size_t untrimmed_count = name.find_last_not_of(' ') + 1;
  for (size_t i = 0; i < base::size(kForbiddenServerNames); ++i) {
    if (name.compare(0, untrimmed_count, kForbiddenServerNames[i]) == 0)
      return true;
  }
  return false;
}

// Compare the values of two EntitySpecifics, accounting for encryption.
bool AreSpecificsEqual(const Cryptographer* cryptographer,
                       const sync_pb::EntitySpecifics& left,
                       const sync_pb::EntitySpecifics& right) {
  // Note that we can't compare encrypted strings directly as they are seeded
  // with a random value.
  std::string left_plaintext, right_plaintext;
  if (left.has_encrypted()) {
    if (!cryptographer->CanDecrypt(left.encrypted())) {
      NOTREACHED() << "Attempting to compare undecryptable data.";
      return false;
    }
    if (!cryptographer->DecryptToString(left.encrypted(), &left_plaintext)) {
      return false;
    }
  } else {
    left_plaintext = left.SerializeAsString();
  }
  if (right.has_encrypted()) {
    if (!cryptographer->CanDecrypt(right.encrypted())) {
      NOTREACHED() << "Attempting to compare undecryptable data.";
      return false;
    }
    if (!cryptographer->DecryptToString(right.encrypted(), &right_plaintext)) {
      return false;
    }
  } else {
    right_plaintext = right.SerializeAsString();
  }
  if (left_plaintext == right_plaintext) {
    return true;
  }
  return false;
}

}  // namespace syncer
