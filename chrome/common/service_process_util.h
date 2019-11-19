// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SERVICE_PROCESS_UTIL_H_
#define CHROME_COMMON_SERVICE_PROCESS_UTIL_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/process/process.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_server_endpoint.h"

class MultiProcessLock;

#if defined(OS_MACOSX)
#ifdef __OBJC__
@class NSString;
#else
class NSString;
#endif
#endif

namespace base {
class CommandLine;
}

// Return the IPC channel to connect to the service process.
mojo::NamedPlatformChannel::ServerName GetServiceProcessServerName();

// Return a name that is scoped to this instance of the service process. We
// use the user-data-dir as a scoping prefix.
std::string GetServiceProcessScopedName(const std::string& append_str);

#if !defined(OS_MACOSX)
// Return a name that is scoped to this instance of the service process. We
// use the user-data-dir and the version as a scoping prefix.
std::string GetServiceProcessScopedVersionedName(const std::string& append_str);
#endif  // !OS_MACOSX

#if defined(OS_POSIX)
// Attempts to take a lock named |name|. If |waiting| is true then this will
// make multiple attempts to acquire the lock.
// Caller is responsible for ownership of the MultiProcessLock.
MultiProcessLock* TakeNamedLock(const std::string& name, bool waiting);
#endif

// The following method is used in a process that acts as a client to the
// service process (typically the browser process). It method checks that if the
// service process is ready to receive IPC commands.
bool CheckServiceProcessReady();

// --------------------------------------------------------------------------

// Forces a service process matching the specified version to shut down.
bool ForceServiceProcessShutdown(const std::string& version,
                                 base::ProcessId process_id);

// Creates command-line to run the service process.
std::unique_ptr<base::CommandLine> CreateServiceProcessCommandLine();

// This is a class that is used by the service process to signal events and
// share data with external clients. This class lives in this file because the
// internal data structures and mechanisms used by the utility methods above
// and this class are shared.
class ServiceProcessState {
 public:
  ServiceProcessState();
  ~ServiceProcessState();

  // Tries to become the sole service process for the current user data dir.
  // Returns false if another service process is already running.
  bool Initialize();

  // Signal that the service process is ready.
  // This method is called when the service process is running and initialized.
  // |terminate_task| is invoked when we get a terminate request from another
  // process (in the same thread that called SignalReady). It can be NULL.
  // |task_runner| must be of type IO and is the loop that POSIX uses
  // to monitor the service process.
  bool SignalReady(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                   const base::Closure& terminate_task);

  // Signal that the service process is stopped.
  void SignalStopped();

  // Register the service process to run on startup.
  bool AddToAutoRun();

  // Unregister the service process to run on startup.
  bool RemoveFromAutoRun();

  // Return the channel handle used for communicating with the service.
#if defined(OS_MACOSX)
  mojo::PlatformChannelServerEndpoint GetServiceProcessServerEndpoint();
#else
  mojo::NamedPlatformChannel::ServerName GetServiceProcessServerName();
#endif

 private:
#if !defined(OS_MACOSX)
  enum ServiceProcessRunningState {
    SERVICE_NOT_RUNNING,
    SERVICE_OLDER_VERSION_RUNNING,
    SERVICE_SAME_VERSION_RUNNING,
    SERVICE_NEWER_VERSION_RUNNING,
  };

  // Create the shared memory data for the service process.
  bool CreateSharedData();

  // If an older version of the service process running, it should be shutdown.
  // Returns false if this process needs to exit.
  bool HandleOtherVersion();

  // Acquires a singleton lock for the service process. A return value of false
  // means that a service process instance is already running.
  bool TakeSingletonLock();

  // Return a name used to name a shared memory file that will be used to locate
  // the currently running service process.
  static std::string GetServiceProcessSharedMemName();

  // Create a writable service process data shared memory region of the
  // specified size. Returns an invalid region on error. If the backing file for
  // the shared memory region already exists but is smaller than |size|, this
  // function may return a valid region which will fail to be mapped.
  static base::WritableSharedMemoryRegion CreateServiceProcessDataRegion(
      size_t size);

  // Open an existing service process data shared memory region of the specified
  // size. Returns an invalid region on error. Note that if the size of the
  // existing region is smaller than |size|, this function may return a valid
  // region which will fail to be mapped. Also note that since the underlying
  // file is writable, the region cannot be read-only.
  static base::ReadOnlySharedMemoryMapping OpenServiceProcessDataMapping(
      size_t size);

  // Deletes a service process data shared memory backing file. Returns false if
  // the file was not able to be deleted.
  static bool DeleteServiceProcessDataRegion();

  static ServiceProcessRunningState GetServiceProcessRunningState(
      std::string* service_version_out,
      base::ProcessId* pid_out);
#endif  // !OS_MACOSX

  // Returns the process id and version of the currently running service
  // process. Note: DO NOT use this check whether the service process is ready
  // because a true return value only means that some process shared data was
  // available, and not that the process is ready to receive IPC commands, or
  // even running.
  static bool GetServiceProcessData(std::string* version, base::ProcessId* pid);

  // Creates the platform specific state.
  void CreateState();

  // Tear down the platform specific state.
  void TearDownState();

  // An opaque object that maintains state. The actual definition of this is
  // platform dependent.
  struct StateData;
  StateData* state_;

#if !defined(OS_MACOSX)
  // The shared memory mapping backing the shared state on non-macos
  // platforms. This is actually referring to named shared memory, and on some
  // platforms (eg, Windows) determines the lifetime of when consumers are able
  // to open the named shared region. This means this region must stay alive
  // for the named region to be visible.
  base::WritableSharedMemoryRegion service_process_data_region_;
#endif

  std::unique_ptr<base::CommandLine> autorun_command_line_;

#if defined(OS_MACOSX)
  friend bool CheckServiceProcessReady();
#endif

  FRIEND_TEST_ALL_PREFIXES(ServiceProcessStateTest, SharedMem);
  FRIEND_TEST_ALL_PREFIXES(ServiceProcessStateTest, ForceShutdown);

  friend class ServiceProcessControlBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceProcessControlBrowserTest, ForceShutdown);
  FRIEND_TEST_ALL_PREFIXES(ServiceProcessControlBrowserTest, CheckPid);
};

#endif  // CHROME_COMMON_SERVICE_PROCESS_UTIL_H_
