// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_BROKER_ENGINE_FILE_REQUESTS_IMPL_H_
#define CHROME_CHROME_CLEANER_ENGINES_BROKER_ENGINE_FILE_REQUESTS_IMPL_H_

#include "chrome/chrome_cleaner/engines/broker/interface_metadata_observer.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/mojom/engine_file_requests.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace chrome_cleaner {

class EngineFileRequestsImpl : public mojom::EngineFileRequests {
 public:
  EngineFileRequestsImpl(
      scoped_refptr<MojoTaskRunner> mojo_task_runner,
      InterfaceMetadataObserver* metadata_observer = nullptr);
  ~EngineFileRequestsImpl() override;

  void Bind(mojo::PendingAssociatedRemote<mojom::EngineFileRequests>* remote);

  // mojom::EngineFileRequests
  void SandboxFindFirstFile(
      const base::FilePath& file_name,
      SandboxFindFirstFileCallback result_callback) override;
  void SandboxFindNextFile(
      mojom::FindHandlePtr handle_ptr,
      SandboxFindNextFileCallback result_callback) override;
  void SandboxFindClose(mojom::FindHandlePtr handle_ptr,
                        SandboxFindCloseCallback result_callback) override;
  void SandboxOpenReadOnlyFile(
      const base::FilePath& file_name,
      uint32_t dwFlagsAndAttributes,
      SandboxOpenReadOnlyFileCallback result_callback) override;

 private:
  void FindFirstFile(const base::FilePath& file_name,
                     SandboxFindFirstFileCallback result_callback);
  void FindNextFile(mojom::FindHandlePtr handle_ptr,
                    SandboxFindNextFileCallback result_callback);
  void FindClose(mojom::FindHandlePtr handle_ptr,
                 SandboxFindCloseCallback result_callback);
  void OpenReadOnlyFile(const base::FilePath& file_name,
                        uint32_t dwFlagsAndAttributes,
                        SandboxOpenReadOnlyFileCallback result_callback);

  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  InterfaceMetadataObserver* metadata_observer_ = nullptr;
  mojo::AssociatedReceiver<mojom::EngineFileRequests> receiver_{this};
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_BROKER_ENGINE_FILE_REQUESTS_IMPL_H_
