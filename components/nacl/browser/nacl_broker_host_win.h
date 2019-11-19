// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_BROWSER_NACL_BROKER_HOST_WIN_H_
#define COMPONENTS_NACL_BROWSER_NACL_BROKER_HOST_WIN_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/process/process.h"
#include "content/public/browser/browser_child_process_host_delegate.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace content {
class BrowserChildProcessHost;
}

namespace nacl {

class NaClBrokerHost : public content::BrowserChildProcessHostDelegate {
 public:
  NaClBrokerHost();
  ~NaClBrokerHost() override;

  // This function starts the broker process. It needs to be called
  // before loaders can be launched.
  bool Init();

  // Send a message to the broker process, causing it to launch
  // a Native Client loader process.
  bool LaunchLoader(int launch_id,
                    mojo::ScopedMessagePipeHandle ipc_channel_handle);

  bool LaunchDebugExceptionHandler(int32_t pid,
                                   base::ProcessHandle process_handle,
                                   const std::string& startup_info);

  // Stop the broker process.
  void StopBroker();

  // Returns true if the process has been asked to terminate. If true, this
  // object should no longer be used; it will eventually be destroyed by
  // BrowserChildProcessHostImpl::OnChildDisconnected()
  bool IsTerminating() { return is_terminating_; }

 private:
  // Handler for NaClProcessMsg_LoaderLaunched message
  void OnLoaderLaunched(int launch_id, base::ProcessHandle handle);

  // Handler for NaClProcessMsg_DebugExceptionHandlerLaunched message
  void OnDebugExceptionHandlerLaunched(int32_t pid, bool success);

  // BrowserChildProcessHostDelegate implementation:
  bool OnMessageReceived(const IPC::Message& msg) override;

  std::unique_ptr<content::BrowserChildProcessHost> process_;
  bool is_terminating_;

  DISALLOW_COPY_AND_ASSIGN(NaClBrokerHost);
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_BROWSER_NACL_BROKER_HOST_WIN_H_
