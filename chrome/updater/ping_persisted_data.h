// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_PING_PERSISTED_DATA_H_
#define CHROME_UPDATER_PING_PERSISTED_DATA_H_

#include <memory>

#include "components/update_client/persisted_data.h"

namespace updater {

// This is a lightweight `PersistedData` that has very few dependencies. The
// implementation has `NOTREACHED` for many methods, and is solely expected to
// be used for standalone pings.
std::unique_ptr<update_client::PersistedData> CreatePingPersistedData();

}  // namespace updater

#endif  // CHROME_UPDATER_PING_PERSISTED_DATA_H_
