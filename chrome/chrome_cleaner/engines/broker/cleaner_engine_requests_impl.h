// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_BROKER_CLEANER_ENGINE_REQUESTS_IMPL_H_
#define CHROME_CHROME_CLEANER_ENGINES_BROKER_CLEANER_ENGINE_REQUESTS_IMPL_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "chrome/chrome_cleaner/engines/broker/cleaner_sandbox_interface.h"
#include "chrome/chrome_cleaner/engines/broker/interface_metadata_observer.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/mojom/cleaner_engine_requests.mojom.h"
#include "chrome/chrome_cleaner/os/file_remover_api.h"
#include "chrome/chrome_cleaner/zip_archiver/zip_archiver.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace chrome_cleaner {

std::unique_ptr<chrome_cleaner::FileRemoverAPI>
CreateFileRemoverWithDigestVerifier(
    std::unique_ptr<ZipArchiver> archiver,
    const base::RepeatingClosure& reboot_needed_callback);

class CleanerEngineRequestsImpl : public mojom::CleanerEngineRequests {
 public:
  CleanerEngineRequestsImpl(
      scoped_refptr<MojoTaskRunner> mojo_task_runner,
      InterfaceMetadataObserver* metadata_observer,
      std::unique_ptr<chrome_cleaner::FileRemoverAPI> file_remover);
  ~CleanerEngineRequestsImpl() override;

  void Bind(
      mojo::PendingAssociatedRemote<mojom::CleanerEngineRequests>* remote);

  // mojom::CleanerEngineRequests
  void SandboxDeleteFile(const base::FilePath& file_name,
                         SandboxDeleteFileCallback result_callback) override;
  void SandboxDeleteFilePostReboot(
      const base::FilePath& file_name,
      SandboxDeleteFilePostRebootCallback result_callback) override;
  void SandboxNtDeleteRegistryKey(
      const WStringEmbeddedNulls& key,
      SandboxNtDeleteRegistryKeyCallback result_callback) override;
  void SandboxNtDeleteRegistryValue(
      const WStringEmbeddedNulls& key,
      const WStringEmbeddedNulls& value_name,
      SandboxNtDeleteRegistryValueCallback result_callback) override;
  void SandboxNtChangeRegistryValue(
      const WStringEmbeddedNulls& key,
      const WStringEmbeddedNulls& value_name,
      const WStringEmbeddedNulls& new_value,
      SandboxNtChangeRegistryValueCallback result_callback) override;
  void SandboxDeleteService(
      const std::wstring& name,
      SandboxDeleteServiceCallback result_callback) override;
  void SandboxDeleteTask(const std::wstring& name,
                         SandboxDeleteServiceCallback result_callback) override;
  void SandboxTerminateProcess(
      uint32_t process_id,
      SandboxTerminateProcessCallback result_callback) override;

 private:
  bool NtDeleteRegistryKey(const WStringEmbeddedNulls& key);
  bool NtDeleteRegistryValue(const WStringEmbeddedNulls& key,
                             const WStringEmbeddedNulls& value_name);
  bool NtChangeRegistryValue(const WStringEmbeddedNulls& key,
                             const WStringEmbeddedNulls& value_name,
                             const WStringEmbeddedNulls& new_value);
  bool DeleteService(const std::wstring& name);
  bool DeleteTask(const std::wstring& name);
  bool TerminateProcess(uint32_t process_id);

  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  mojo::AssociatedReceiver<mojom::CleanerEngineRequests> receiver_{this};
  [[maybe_unused]] InterfaceMetadataObserver* metadata_observer_ = nullptr;
  std::unique_ptr<chrome_cleaner::FileRemoverAPI> file_remover_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_BROKER_CLEANER_ENGINE_REQUESTS_IMPL_H_
