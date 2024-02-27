// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_TEST_TEST_KEY_DATA_PROVIDER_H_
#define COMPONENTS_METRICS_STRUCTURED_TEST_TEST_KEY_DATA_PROVIDER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "components/metrics/structured/lib/key_data_provider.h"

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
class TestKeyDataProvider : public KeyDataProvider, KeyDataProvider::Observer {
 public:
  explicit TestKeyDataProvider(const base::FilePath& device_key_path);
  TestKeyDataProvider(const base::FilePath& device_key_path,
                      const base::FilePath& profile_key_path);
  ~TestKeyDataProvider() override;

  // KeyDataProvider:
  bool IsReady() override;
  std::optional<uint64_t> GetId(const std::string& project_name) override;
  std::optional<uint64_t> GetSecondaryId(
      const std::string& project_name) override;
  KeyData* GetKeyData(const std::string& project_name) override;
  void Purge() override;

  // KeyDataProvider::Observer
  void OnKeyReady() override;

  void OnProfileAdded(const base::FilePath& profile_path);

 private:
  base::FilePath device_key_path_;
  base::FilePath profile_key_path_;

  std::unique_ptr<KeyDataProvider> device_key_data_;
  std::unique_ptr<KeyDataProvider> profile_key_data_;
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_TEST_TEST_KEY_DATA_PROVIDER_H_
