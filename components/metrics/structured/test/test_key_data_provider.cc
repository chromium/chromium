// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/test/test_key_data_provider.h"

#include "base/check.h"
#include "base/time/time.h"

namespace metrics::structured {

TestKeyDataProvider::TestKeyDataProvider(const base::FilePath& device_key_path)
    : device_key_path_(device_key_path), profile_key_path_(base::FilePath()) {}

TestKeyDataProvider::TestKeyDataProvider(const base::FilePath& device_key_path,
                                         const base::FilePath& profile_key_path)
    : device_key_path_(device_key_path), profile_key_path_(profile_key_path) {}

TestKeyDataProvider::~TestKeyDataProvider() = default;

KeyData* TestKeyDataProvider::GetDeviceKeyData() {
  DCHECK(HasDeviceKey());

  return device_key_data_.get();
}

KeyData* TestKeyDataProvider::GetProfileKeyData() {
  DCHECK(HasProfileKey());

  return profile_key_data_.get();
}

bool TestKeyDataProvider::HasProfileKey() {
  return profile_key_data_ != nullptr;
}

bool TestKeyDataProvider::HasDeviceKey() {
  return device_key_data_ != nullptr;
}

void TestKeyDataProvider::InitializeDeviceKey(base::OnceClosure callback) {
  DCHECK(!device_key_path_.empty());

  device_key_data_ = std::make_unique<KeyData>(
      device_key_path_, base::Milliseconds(0), std::move(callback));
}

void TestKeyDataProvider::InitializeProfileKey(
    const base::FilePath& profile_path,
    base::OnceClosure callback) {
  // If the profile path has not been set, then set it here.
  if (profile_key_path_.empty()) {
    profile_key_path_ = profile_path;
  }

  DCHECK(!profile_key_path_.empty());

  profile_key_data_ = std::make_unique<KeyData>(
      profile_key_path_, base::Milliseconds(0), std::move(callback));
}

void TestKeyDataProvider::Purge() {
  if (HasProfileKey()) {
    GetProfileKeyData()->Purge();
  }

  if (HasDeviceKey()) {
    GetDeviceKeyData()->Purge();
  }
}

}  // namespace metrics::structured
