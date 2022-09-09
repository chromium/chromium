// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_TARGET_ENGINE_FILE_REQUESTS_PROXY_H_
#define CHROME_CHROME_CLEANER_ENGINES_TARGET_ENGINE_FILE_REQUESTS_PROXY_H_

#include <stdint.h>
#include <windows.h>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "base/win/scoped_handle.h"
#include "chrome/chrome_cleaner/engines/target/sandbox_request_helper.h"
#include "chrome/chrome_cleaner/mojom/engine_file_requests.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace chrome_cleaner {

typedef int64_t FindFileHandle;

// Accessors to send the file requests over the Mojo connection.
class EngineFileRequestsProxy
    : public base::RefCountedThreadSafe<EngineFileRequestsProxy> {
 public:
  EngineFileRequestsProxy(
      mojo::PendingAssociatedRemote<mojom::EngineFileRequests> file_requests,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Implements synchronous callbacks to be called on arbitrary threads from the
  // engine.
  virtual uint32_t FindFirstFile(const base::FilePath& path,
                                 LPWIN32_FIND_DATAW lpFindFileData,
                                 FindFileHandle* handle);
  virtual uint32_t FindNextFile(FindFileHandle hFindFile,
                                LPWIN32_FIND_DATAW lpFindFileData);
  virtual uint32_t FindClose(FindFileHandle hFindFile);
  virtual base::win::ScopedHandle OpenReadOnlyFile(
      const base::FilePath& path,
      uint32_t dwFlagsAndAttributes);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
    return task_runner_;
  }

  void UnbindRequestsRemote();

 protected:
  // Tests can subclass this create a proxy that's not bound to anything.
  EngineFileRequestsProxy();

  virtual ~EngineFileRequestsProxy();

 private:
  friend class base::RefCountedThreadSafe<EngineFileRequestsProxy>;
  friend class TestEngineRequestInvoker;

  // Invokes the desired function call from the IPC thread.
  MojoCallStatus SandboxFindFirstFile(
      const base::FilePath& path,
      mojom::EngineFileRequests::SandboxFindFirstFileCallback result_callback);
  MojoCallStatus SandboxFindNextFile(
      FindFileHandle handle,
      mojom::EngineFileRequests::SandboxFindNextFileCallback result_callback);
  MojoCallStatus SandboxFindClose(
      FindFileHandle handle,
      mojom::EngineFileRequests::SandboxFindCloseCallback result_callback);
  MojoCallStatus SandboxOpenReadOnlyFile(
      const base::FilePath& path,
      uint32_t dwFlagsAndAttributes,
      mojom::EngineFileRequests::SandboxOpenReadOnlyFileCallback
          result_callback);

  // A EngineFileRequests that will send the requests over the Mojo
  // connection.
  mojo::AssociatedRemote<mojom::EngineFileRequests> file_requests_;

  // A task runner for the IPC thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_TARGET_ENGINE_FILE_REQUESTS_PROXY_H_
