// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/common/connections_manager/fake_nearby_connection.h"

FakeNearbyConnection::FakeNearbyConnection() = default;
FakeNearbyConnection::~FakeNearbyConnection() {
  if (!closed_)
    Close();
}

void FakeNearbyConnection::Read(ReadCallback callback) {
  DCHECK(!closed_);
  callback_ = std::move(callback);
  MaybeRunCallback();
}

void FakeNearbyConnection::Write(std::vector<uint8_t> bytes) {
  DCHECK(!closed_);
  write_data_.push(std::move(bytes));
}

void FakeNearbyConnection::Close() {
  DCHECK(!closed_);
  closed_ = true;
  if (disconnect_listener_)
    std::move(disconnect_listener_).Run();

  if (callback_) {
    has_read_callback_been_run_ = true;
    std::move(callback_).Run(std::nullopt);
  }
}

void FakeNearbyConnection::SetDisconnectionListener(
    base::OnceClosure listener) {
  DCHECK(!closed_);
  disconnect_listener_ = std::move(listener);
}

void FakeNearbyConnection::AppendReadableData(std::vector<uint8_t> bytes) {
  DCHECK(!closed_);
  read_data_.push(std::move(bytes));
  MaybeRunCallback();
}

std::vector<uint8_t> FakeNearbyConnection::GetWrittenData() {
  if (write_data_.empty())
    return {};

  std::vector<uint8_t> bytes = std::move(write_data_.front());
  write_data_.pop();
  return bytes;
}

bool FakeNearbyConnection::IsClosed() {
  return closed_;
}

void FakeNearbyConnection::MaybeRunCallback() {
  DCHECK(!closed_);
  if (!callback_ || read_data_.empty())
    return;
  auto item = std::move(read_data_.front());
  read_data_.pop();
  has_read_callback_been_run_ = true;
  std::move(callback_).Run(std::move(item));
}

void FakeNearbyConnection::InvokeEmptyReadCallback() {
  has_read_callback_been_run_ = true;
  std::move(callback_).Run({});
}
