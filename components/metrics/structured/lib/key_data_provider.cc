// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/lib/key_data_provider.h"

#include <optional>
#include <string>

namespace metrics::structured {

KeyDataProvider::KeyDataProvider() = default;
KeyDataProvider::~KeyDataProvider() = default;

void KeyDataProvider::AddObserver(KeyDataProvider::Observer* observer) {
  observers_.AddObserver(observer);
}

void KeyDataProvider::RemoveObserver(KeyDataProvider::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void KeyDataProvider::NotifyKeyReady() {
  for (Observer& obs : observers_) {
    obs.OnKeyReady();
  }
}

std::optional<uint64_t> KeyDataProvider::GetSecondaryId(
    const std::string& project_name) {
  return std::nullopt;
}

}  // namespace metrics::structured
