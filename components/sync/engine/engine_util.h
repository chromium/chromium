// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_ENGINE_UTIL_H_
#define COMPONENTS_SYNC_ENGINE_ENGINE_UTIL_H_

// The functions defined are shared among some of the classes that implement
// the internal sync API.  They are not to be used by clients of the API.

#include <memory>
#include <string>

namespace sync_pb {
class EntitySpecifics;
class PasswordSpecificsData;
class WifiConfigurationSpecificsData;
}

namespace syncer {

class Cryptographer;

std::unique_ptr<sync_pb::PasswordSpecificsData> DecryptPasswordSpecifics(
    const sync_pb::EntitySpecifics& specifics,
    const Cryptographer* crypto);

std::unique_ptr<sync_pb::WifiConfigurationSpecificsData>
DecryptWifiConfigurationSpecifics(const sync_pb::EntitySpecifics& specifics,
                                  const Cryptographer* crypto);

void SyncAPINameToServerName(const std::string& syncer_name, std::string* out);
void ServerNameToSyncAPIName(const std::string& server_name, std::string* out);

bool IsNameServerIllegalAfterTrimming(const std::string& name);

bool AreSpecificsEqual(const Cryptographer* cryptographer,
                       const sync_pb::EntitySpecifics& left,
                       const sync_pb::EntitySpecifics& right);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_ENGINE_UTIL_H_
