// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_BROWSER_NACL_BROKER_SERVICE_WIN_H_
#define COMPONENTS_NACL_BROWSER_NACL_BROKER_SERVICE_WIN_H_

#include <stdint.h>

#include <map>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "components/nacl/browser/nacl_broker_host_win.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace nacl {

class NaClProcessHost;

class NaClBrokerService {
 public:
  // Returns the NaClBrokerService singleton.
  static NaClBrokerService* GetInstance();

  // Can be called several times, must be called before LaunchLoader.
  bool StartBroker();

  // Send a message to the broker process, causing it to launch
  // a Native Client loader process.
  bool LaunchLoader(base::WeakPtr<NaClProcessHost> client,
                    mojo::ScopedMessagePipeHandle ipc_channel_handle);

  // Called by NaClBrokerHost to notify the service that a loader was launched.
  void OnLoaderLaunched(int launch_id, base::Process process);

  // Called by NaClProcessHost when a loader process is terminated
  void OnLoaderDied();

  bool LaunchDebugExceptionHandler(base::WeakPtr<NaClProcessHost> client,
                                   int32_t pid,
                                   base::ProcessHandle process_handle,
                                   const std::string& startup_info);

  // Called by NaClBrokerHost to notify the service that a debug
  // exception handler was started.
  void OnDebugExceptionHandlerLaunched(int32_t pid, bool success);

 private:
  typedef std::map<int, base::WeakPtr<NaClProcessHost>> PendingLaunchesMap;
  typedef std::map<int, base::WeakPtr<NaClProcessHost>>
      PendingDebugExceptionHandlersMap;

  friend struct base::DefaultSingletonTraits<NaClBrokerService>;

  NaClBrokerService();
  ~NaClBrokerService();

  NaClBrokerHost* GetBrokerHost();

  int loaders_running_;
  int next_launch_id_ = 0;
  PendingLaunchesMap pending_launches_;
  PendingDebugExceptionHandlersMap pending_debuggers_;

  DISALLOW_COPY_AND_ASSIGN(NaClBrokerService);
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_BROWSER_NACL_BROKER_SERVICE_WIN_H_
