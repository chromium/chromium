// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_STRUCTURED_KEY_DATA_PROVIDER_FILE_H_
#define COMPONENTS_METRICS_STRUCTURED_KEY_DATA_PROVIDER_FILE_H_

#include <memory>
#include <optional>

#include "base/barrier_closure.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/metrics/structured/lib/key_data_provider.h"

namespace metrics::structured {

// KeyDataProvider implementation that stores the keys in a file.
//
// (b/316198668): Explore to remove this layer of abstraction since it should
// not be needed anymore.
class KeyDataProviderFile : public KeyDataProvider, KeyDataProvider::Observer {
 public:
  KeyDataProviderFile(const base::FilePath& file_path,
                      base::TimeDelta write_delay);
  ~KeyDataProviderFile() override;

  // KeyDataProvider:
  bool IsReady() override;
  std::optional<uint64_t> GetId(const std::string& project_name) override;
  std::optional<uint64_t> GetSecondaryId(
      const std::string& project_name) override;
  KeyData* GetKeyData(const std::string& project_name) override;
  void Purge() override;

  // KeyDataProvider::Observer:
  void OnKeyReady() override;

 private:
  const base::FilePath file_path_;
  const base::TimeDelta write_delay_;
  bool is_data_loaded_ = false;

  std::unique_ptr<KeyData> key_data_;
  base::OnceClosure on_key_ready_callback_;

  base::WeakPtrFactory<KeyDataProviderFile> weak_ptr_factory_{this};
};

}  // namespace metrics::structured

#endif  // COMPONENTS_METRICS_STRUCTURED_KEY_DATA_PROVIDER_FILE_H_
