// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CONNECTIONS_MANAGER_FAKE_NEARBY_CONNECTION_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CONNECTIONS_MANAGER_FAKE_NEARBY_CONNECTION_H_

#include <queue>
#include <vector>

#include "chromeos/ash/components/nearby/common/connections_manager/nearby_connection.h"

class FakeNearbyConnection : public NearbyConnection {
 public:
  FakeNearbyConnection();
  ~FakeNearbyConnection() override;

  // NearbyConnection:
  void Read(ReadCallback callback) override;
  void Write(std::vector<uint8_t> bytes) override;
  void Close() override;
  void SetDisconnectionListener(base::OnceClosure listener) override;

  void AppendReadableData(std::vector<uint8_t> bytes);
  std::vector<uint8_t> GetWrittenData();
  void InvokeEmptyReadCallback();

  bool IsClosed();
  bool has_read_callback_been_run() { return has_read_callback_been_run_; }

 private:
  void MaybeRunCallback();

  bool closed_ = false;
  bool has_read_callback_been_run_ = false;

  ReadCallback callback_;
  std::queue<std::vector<uint8_t>> read_data_;
  std::queue<std::vector<uint8_t>> write_data_;
  base::OnceClosure disconnect_listener_;
};

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CONNECTIONS_MANAGER_FAKE_NEARBY_CONNECTION_H_
