// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dissidia/fake_dissidia_client.h"

#include <utility>

namespace chromeos {

FakeDissidiaClient::FakeDissidiaClient() = default;

FakeDissidiaClient::~FakeDissidiaClient() = default;

void FakeDissidiaClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeDissidiaClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeDissidiaClient::PerformUpdate(const std::string& target,
                                       PerformUpdateCallback callback) {
  last_target_ = target;
  perform_update_call_count_++;
  std::move(callback).Run(update_status_, update_message_);
}

void FakeDissidiaClient::NotifyProgress(int32_t percent,
                                        const std::string& stage) {
  for (auto& observer : observers_) {
    observer.OnProgress(percent, stage);
  }
}

void FakeDissidiaClient::NotifyCompleted(
    bool success,
    dissidia::CompletedErrorCode error_code,
    const std::string& message) {
  for (auto& observer : observers_) {
    observer.OnCompleted(success, error_code, message);
  }
}

}  // namespace chromeos
