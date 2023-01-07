// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_TARGET_CLEANER_ENGINE_REQUESTS_PROXY_H_
#define CHROME_CHROME_CLEANER_ENGINES_TARGET_CLEANER_ENGINE_REQUESTS_PROXY_H_

#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/chrome_cleaner/engines/target/sandbox_request_helper.h"
#include "chrome/chrome_cleaner/mojom/cleaner_engine_requests.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace chrome_cleaner {

class CleanerEngineRequestsProxy
    : public base::RefCountedThreadSafe<CleanerEngineRequestsProxy> {
 public:
  CleanerEngineRequestsProxy(
      mojo::PendingAssociatedRemote<mojom::CleanerEngineRequests> requests,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Implements synchronous callbacks to be called on arbitrary threads from the
  // engine.
  virtual bool DeleteFile(const base::FilePath& file_name);
  virtual bool DeleteFilePostReboot(const base::FilePath& file_name);
  virtual bool NtDeleteRegistryKey(const WStringEmbeddedNulls& key);
  virtual bool NtDeleteRegistryValue(const WStringEmbeddedNulls& key,
                                     const WStringEmbeddedNulls& value_name);
  virtual bool NtChangeRegistryValue(const WStringEmbeddedNulls& key,
                                     const WStringEmbeddedNulls& value_name,
                                     const WStringEmbeddedNulls& new_value);
  virtual bool DeleteService(const std::wstring& name);
  virtual bool DeleteTask(const std::wstring& name);
  virtual bool TerminateProcess(base::ProcessId process_id);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
    return task_runner_;
  }

  void UnbindRequestsRemote();

 protected:
  // Tests can subclass this create a proxy that's not bound to anything.
  CleanerEngineRequestsProxy();

  virtual ~CleanerEngineRequestsProxy();

 private:
  friend class base::RefCountedThreadSafe<CleanerEngineRequestsProxy>;
  friend class TestEngineRequestInvoker;

  MojoCallStatus SandboxDeleteFile(
      const base::FilePath& file_name,
      mojom::CleanerEngineRequests::SandboxDeleteFileCallback result_callback);
  MojoCallStatus SandboxDeleteFilePostReboot(
      const base::FilePath& file_name,
      mojom::CleanerEngineRequests::SandboxDeleteFilePostRebootCallback
          result_callback);
  MojoCallStatus SandboxNtDeleteRegistryKey(
      const WStringEmbeddedNulls& key,
      mojom::CleanerEngineRequests::SandboxNtDeleteRegistryKeyCallback
          result_callback);
  MojoCallStatus SandboxNtDeleteRegistryValue(
      const WStringEmbeddedNulls& key,
      const WStringEmbeddedNulls& value_name,
      mojom::CleanerEngineRequests::SandboxNtDeleteRegistryValueCallback
          result_callback);
  MojoCallStatus SandboxNtChangeRegistryValue(
      const WStringEmbeddedNulls& key,
      const WStringEmbeddedNulls& value_name,
      const WStringEmbeddedNulls& new_value,
      mojom::CleanerEngineRequests::SandboxNtChangeRegistryValueCallback
          result_callback);
  MojoCallStatus SandboxDeleteService(
      const std::wstring& name,
      mojom::CleanerEngineRequests::SandboxDeleteServiceCallback
          result_callback);
  MojoCallStatus SandboxDeleteTask(
      const std::wstring& name,
      mojom::CleanerEngineRequests::SandboxDeleteTaskCallback result_callback);
  MojoCallStatus SandboxTerminateProcess(
      uint32_t process_id,
      mojom::CleanerEngineRequests::SandboxTerminateProcessCallback
          result_callback);

  // A CleanerEngineRequests that will send the requests over the Mojo
  // connection.
  mojo::AssociatedRemote<mojom::CleanerEngineRequests> requests_;

  // A task runner for the IPC thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_TARGET_CLEANER_ENGINE_REQUESTS_PROXY_H_
