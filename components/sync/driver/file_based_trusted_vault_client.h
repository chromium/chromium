// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_FILE_BASED_TRUSTED_VAULT_CLIENT_H_
#define COMPONENTS_SYNC_DRIVER_FILE_BASED_TRUSTED_VAULT_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "components/sync/driver/trusted_vault_client.h"

namespace syncer {

// Standalone, file-based implementation of TrustedVaultClient that stores the
// keys in a local file, containing a serialized protocol buffer encrypted with
// platform-dependent crypto mechanisms (OSCrypt).
//
// Reading of the file is done lazily.
class FileBasedTrustedVaultClient : public TrustedVaultClient {
 public:
  explicit FileBasedTrustedVaultClient(const base::FilePath& file_path);
  ~FileBasedTrustedVaultClient() override;

  // TrustedVaultClient implementation.
  void FetchKeys(
      const std::string& gaia_id,
      base::OnceCallback<void(const std::vector<std::string>&)> cb) override;
  void StoreKeys(const std::string& gaia_id,
                 const std::vector<std::string>& keys) override;

  // Runs |cb| when all requests have completed.
  void WaitForFlushForTesting(base::OnceClosure cb) const;
  bool IsInitializationTriggeredForTesting() const;

 private:
  void TriggerLazyInitializationIfNeeded();

  const base::FilePath file_path_;
  const scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // Backend constructed lazily in the UI thread, used in |backend_task_runner_|
  // and destroyed (refcounted) on any thread.
  class Backend;
  scoped_refptr<Backend> backend_;

  DISALLOW_COPY_AND_ASSIGN(FileBasedTrustedVaultClient);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_FILE_BASED_TRUSTED_VAULT_CLIENT_H_
