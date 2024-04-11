// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connection.h"

NearbyConnection::NearbyConnection() = default;

NearbyConnection::~NearbyConnection() = default;

base::WeakPtr<NearbyConnection> NearbyConnection::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
