// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_TARGET_CLEANER_ENGINE_REQUESTS_PROXY_H_
#define CHROME_CHROME_CLEANER_ENGINES_TARGET_CLEANER_ENGINE_REQUESTS_PROXY_H_

#include "base/memory/ref_counted.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
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
  virtual bool NtDeleteRegistryKey(const String16EmbeddedNulls& key);
  virtual bool NtDeleteRegistryValue(const String16EmbeddedNulls& key,
                                     const String16EmbeddedNulls& value_name);
  virtual bool NtChangeRegistryValue(const String16EmbeddedNulls& key,
                                     const String16EmbeddedNulls& value_name,
                                     const String16EmbeddedNulls& new_value);
  virtual bool DeleteService(const base::string16& name);
  virtual bool DeleteTask(const base::string16& name);
  virtual bool TerminateProcess(base::ProcessId process_id);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
    return task_runner_;
  }

  void UnbindRequestsRemote();

 protected:
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
      const String16EmbeddedNulls& key,
      mojom::CleanerEngineRequests::SandboxNtDeleteRegistryKeyCallback
          result_callback);
  MojoCallStatus SandboxNtDeleteRegistryValue(
      const String16EmbeddedNulls& key,
      const String16EmbeddedNulls& value_name,
      mojom::CleanerEngineRequests::SandboxNtDeleteRegistryValueCallback
          result_callback);
  MojoCallStatus SandboxNtChangeRegistryValue(
      const String16EmbeddedNulls& key,
      const String16EmbeddedNulls& value_name,
      const String16EmbeddedNulls& new_value,
      mojom::CleanerEngineRequests::SandboxNtChangeRegistryValueCallback
          result_callback);
  MojoCallStatus SandboxDeleteService(
      const base::string16& name,
      mojom::CleanerEngineRequests::SandboxDeleteServiceCallback
          result_callback);
  MojoCallStatus SandboxDeleteTask(
      const base::string16& name,
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
