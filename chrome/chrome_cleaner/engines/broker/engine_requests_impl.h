// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_BROKER_ENGINE_REQUESTS_IMPL_H_
#define CHROME_CHROME_CLEANER_ENGINES_BROKER_ENGINE_REQUESTS_IMPL_H_

#include <vector>

#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "chrome/chrome_cleaner/engines/broker/interface_metadata_observer.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/mojom/engine_requests.mojom.h"
#include "chrome/chrome_cleaner/strings/string16_embedded_nulls.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace chrome_cleaner {

class EngineRequestsImpl : public mojom::EngineRequests {
 public:
  EngineRequestsImpl(scoped_refptr<MojoTaskRunner> mojo_task_runner,
                     InterfaceMetadataObserver* metadata_observer = nullptr);
  ~EngineRequestsImpl() override;

  void Bind(mojo::PendingAssociatedRemote<mojom::EngineRequests>* remote);

  // mojom::EngineRequests
  void SandboxGetFileAttributes(
      const base::FilePath& file_name,
      SandboxGetFileAttributesCallback result_callback) override;
  void SandboxGetKnownFolderPath(
      mojom::KnownFolder folder_id,
      SandboxGetKnownFolderPathCallback result_callback) override;
  void SandboxGetProcesses(
      SandboxGetProcessesCallback result_callback) override;
  void SandboxGetTasks(SandboxGetTasksCallback result_callback) override;
  void SandboxGetProcessImagePath(
      base::ProcessId pid,
      SandboxGetProcessImagePathCallback result_callback) override;
  void SandboxGetLoadedModules(
      base::ProcessId pid,
      SandboxGetLoadedModulesCallback result_callback) override;
  void SandboxGetProcessCommandLine(
      base::ProcessId pid,
      SandboxGetProcessCommandLineCallback result_callback) override;
  void SandboxGetUserInfoFromSID(
      mojom::StringSidPtr string_sid,
      SandboxGetUserInfoFromSIDCallback result_callback) override;
  void SandboxOpenReadOnlyRegistry(
      HANDLE root_key_handle,
      const base::string16& sub_key,
      uint32_t dw_access,
      SandboxOpenReadOnlyRegistryCallback result_callback) override;
  void SandboxNtOpenReadOnlyRegistry(
      HANDLE root_key_handle,
      const String16EmbeddedNulls& sub_key,
      uint32_t dw_access,
      SandboxNtOpenReadOnlyRegistryCallback result_callback) override;

 private:
  void GetFileAttributes(const base::FilePath& file_name,
                         SandboxGetFileAttributesCallback result_callback);
  void GetKnownFolderPath(mojom::KnownFolder folder_id,
                          SandboxGetKnownFolderPathCallback result_callback);
  void GetProcesses(SandboxGetProcessesCallback result_callback);
  void GetTasks(SandboxGetTasksCallback result_callback);
  void GetProcessImagePath(base::ProcessId pid,
                           SandboxGetProcessImagePathCallback result_callback);
  void GetLoadedModules(base::ProcessId pid,
                        SandboxGetLoadedModulesCallback result_callback);
  void GetProcessCommandLine(
      base::ProcessId pid,
      SandboxGetProcessCommandLineCallback result_callback);
  void GetUserInfoFromSID(mojom::StringSidPtr string_sid,
                          SandboxGetUserInfoFromSIDCallback result_callback);
  void OpenReadOnlyRegistry(
      HANDLE root_key_handle,
      const base::string16& sub_key,
      uint32_t dw_access,
      SandboxOpenReadOnlyRegistryCallback result_callback);
  void NtOpenReadOnlyRegistry(
      HANDLE root_key_handle,
      const String16EmbeddedNulls& sub_key,
      uint32_t dw_access,
      SandboxNtOpenReadOnlyRegistryCallback result_callback);

  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  InterfaceMetadataObserver* metadata_observer_ = nullptr;
  mojo::AssociatedReceiver<mojom::EngineRequests> receiver_{this};
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_BROKER_ENGINE_REQUESTS_IMPL_H_
