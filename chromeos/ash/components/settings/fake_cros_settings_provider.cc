// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/settings/fake_cros_settings_provider.h"

namespace ash {

FakeCrosSettingsProvider::FakeCrosSettingsProvider(
    const NotifyObserversCallback& notify_cb)
    : CrosSettingsProvider(notify_cb) {}
FakeCrosSettingsProvider::~FakeCrosSettingsProvider() = default;

const base::Value* FakeCrosSettingsProvider::Get(std::string_view path) const {
  const base::Value* value;
  if (map_.GetValue(path, &value)) {
    return value;
  }
  return nullptr;
}

CrosSettingsProvider::TrustedStatus
FakeCrosSettingsProvider::PrepareTrustedValues(base::OnceClosure* callback) {
  if (trusted_status_ == TEMPORARILY_UNTRUSTED) {
    callbacks_.push_back(std::move(*callback));
  }
  return trusted_status_;
}

bool FakeCrosSettingsProvider::HandlesSetting(std::string_view path) const {
  return map_.GetValue(path, nullptr);
}

void FakeCrosSettingsProvider::SetTrustedStatus(TrustedStatus status) {
  trusted_status_ = status;
  if (trusted_status_ != TEMPORARILY_UNTRUSTED) {
    for (base::OnceClosure& callback :
         std::exchange(callbacks_, std::vector<base::OnceClosure>())) {
      std::move(callback).Run();
    }
  }
}

void FakeCrosSettingsProvider::Set(std::string_view path, base::Value value) {
  if (map_.SetValue(path, std::move(value))) {
    NotifyObservers(std::string(path));
  }
}

void FakeCrosSettingsProvider::Set(std::string_view path, bool value) {
  Set(path, base::Value(value));
}

void FakeCrosSettingsProvider::Set(std::string_view path, int value) {
  Set(path, base::Value(value));
}

void FakeCrosSettingsProvider::Set(std::string_view path, double value) {
  Set(path, base::Value(value));
}

void FakeCrosSettingsProvider::Set(std::string_view path,
                                   std::string_view value) {
  Set(path, base::Value(value));
}

}  // namespace ash
