// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PROCESS_PROXY_PROCESS_PROXY_REGISTRY_H_
#define CHROMEOS_PROCESS_PROXY_PROCESS_PROXY_REGISTRY_H_

#include <map>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "chromeos/process_proxy/process_proxy.h"

namespace chromeos {

// Keeps track of all created ProcessProxies. It is created lazily and should
// live on a single thread (where all methods must be called).
class COMPONENT_EXPORT(CHROMEOS_PROCESS_PROXY) ProcessProxyRegistry {
 public:
  using OutputCallback =
      base::RepeatingCallback<void(const std::string& id,
                                   const std::string& output_type,
                                   const std::string& output_data)>;

  // Info we need about a ProcessProxy instance.
  struct ProcessProxyInfo {
    scoped_refptr<ProcessProxy> proxy;
    OutputCallback callback;

    ProcessProxyInfo();
    // This is to make map::insert happy, we don't init anything.
    ProcessProxyInfo(const ProcessProxyInfo& other);
    ~ProcessProxyInfo();
  };

  static ProcessProxyRegistry* Get();

  ProcessProxyRegistry(const ProcessProxyRegistry&) = delete;
  ProcessProxyRegistry& operator=(const ProcessProxyRegistry&) = delete;

  // Converts the id returned by OpenProcess() to the system pid.
  static int ConvertToSystemPID(const std::string& id);

  // Returns a SequencedTaskRunner where the singleton instance of
  // ProcessProxyRegistry lives.
  static scoped_refptr<base::SequencedTaskRunner> GetTaskRunner();

  // Starts new ProcessProxy (which starts new process).
  // Returns true if the process is created successfully, false otherwise.
  // The unique process id is passed back via |id|.
  bool OpenProcess(const base::CommandLine& cmdline,
                   const std::string& user_id_hash,
                   const OutputCallback& callback,
                   std::string* id);
  // Sends data to the process identified by |id|.
  void SendInput(const std::string& id,
                 const std::string& data,
                 base::OnceCallback<void(bool)> callback);
  // Stops the process identified by |id|.
  bool CloseProcess(const std::string& id);
  // Reports terminal resize to process proxy.
  bool OnTerminalResize(const std::string& id, int width, int height);
  // Notifies process proxy identified by |id| that previously reported output
  // has been handled.
  void AckOutput(const std::string& id);

  // Shuts down registry, closing all associated processed.
  void ShutDown();

  // Get the process for testing purposes.
  const base::Process* GetProcessForTesting(const std::string& id);

 private:
  friend struct ::base::LazyInstanceTraitsBase<ProcessProxyRegistry>;

  ProcessProxyRegistry();
  ~ProcessProxyRegistry();

  // Gets called when output gets detected.
  void OnProcessOutput(const std::string& id,
                       ProcessOutputType type,
                       const std::string& data);

  bool EnsureWatcherThreadStarted();

  // Map of all existing ProcessProxies.
  std::map<std::string, ProcessProxyInfo> proxy_map_;

  std::unique_ptr<base::Thread> watcher_thread_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace chromeos

#endif  // CHROMEOS_PROCESS_PROXY_PROCESS_PROXY_REGISTRY_H_
