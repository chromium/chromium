// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_LOADER_NONSFI_NONSFI_LISTENER_H_
#define COMPONENTS_NACL_LOADER_NONSFI_NONSFI_LISTENER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "components/nacl/common/nacl_types.h"
#include "ipc/ipc_listener.h"

namespace IPC {
class Message;
class SyncChannel;
}  // namespace IPC

class NaClTrustedListener;

namespace nacl {

struct NaClStartParams;

namespace nonsfi {

class NonSfiListener : public IPC::Listener {
 public:
  NonSfiListener();

  NonSfiListener(const NonSfiListener&) = delete;
  NonSfiListener& operator=(const NonSfiListener&) = delete;

  ~NonSfiListener() override;

  // Listen for a request to launch a non-SFI NaCl module.
  void Listen();

 private:
  bool OnMessageReceived(const IPC::Message& msg) override;
  void OnAddPrefetchedResource(
      const nacl::NaClResourcePrefetchResult& prefetched_resource_file);
  void OnStart(const nacl::NaClStartParams& params);

  base::Thread io_thread_;
  base::WaitableEvent shutdown_event_;
  std::unique_ptr<IPC::SyncChannel> channel_;
  std::unique_ptr<NaClTrustedListener> trusted_listener_;

  std::unique_ptr<std::map<std::string, int>> key_fd_map_;
};

}  // namespace nonsfi
}  // namespace nacl

#endif  // COMPONENTS_NACL_LOADER_NONSFI_NONSFI_LISTENER_H_
