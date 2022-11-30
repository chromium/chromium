// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_CONNECTION_STATUS_H_
#define COMPONENTS_SYNC_ENGINE_CONNECTION_STATUS_H_

namespace syncer {

// Status of the sync connection to the server.
enum ConnectionStatus {
  CONNECTION_NOT_ATTEMPTED,
  CONNECTION_OK,
  CONNECTION_AUTH_ERROR,
  CONNECTION_SERVER_ERROR
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_CONNECTION_STATUS_H_
