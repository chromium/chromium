// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_TEST_TEST_KEY_DATA_PROVIDER_H_
#define COMPONENTS_METRICS_STRUCTURED_TEST_TEST_KEY_DATA_PROVIDER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "components/metrics/structured/key_data_provider.h"

namespace base {
class FilePath;
}

namespace metrics::structured {

// Test implementation for KeyDataProvider.
//
// If only the |device_key_path| is provided in the ctor, then
// |profile_key_data_| will be empty until InitializeProfileKey is called and
// created in specified path |profile_path|. If |profile_key_path| is provided
// in the ctor, then |profile_path| provided in InitializeProfileKey will be
// ignored.
class TestKeyDataProvider : public KeyDataProvider {
 public:
  explicit TestKeyDataProvider(const base::FilePath& device_key_path);
  TestKeyDataProvider(const base::FilePath& device_key_path,
                      const base::FilePath& profile_key_path);
  ~TestKeyDataProvider() override;

  // KeyDataProvider:
  KeyData* GetDeviceKeyData() override;
  KeyData* GetProfileKeyData() override;
  bool HasProfileKey() override;
  bool HasDeviceKey() override;
  void InitializeDeviceKey(base::OnceClosure callback) override;
  void InitializeProfileKey(const base::FilePath& profile_path,
                            base::OnceClosure callback) override;
  void Purge() override;

 private:
  base::FilePath device_key_path_;
  base::FilePath profile_key_path_;

  std::unique_ptr<KeyData> device_key_data_;
  std::unique_ptr<KeyData> profile_key_data_;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_TEST_TEST_KEY_DATA_PROVIDER_H_
