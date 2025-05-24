// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_RENDERER_MANIFEST_SERVICE_CHANNEL_H_
#define COMPONENTS_NACL_RENDERER_MANIFEST_SERVICE_CHANNEL_H_

#include <stdint.h>

#include <memory>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "ipc/ipc_listener.h"

namespace base {
class WaitableEvent;
}  // namespace base

namespace IPC {
struct ChannelHandle;
class Message;
class SyncChannel;
}  // namespace IPC

namespace nacl {

class ManifestServiceChannel : public IPC::Listener {
 public:
  typedef base::OnceCallback<void(base::File, uint64_t, uint64_t)>
      OpenResourceCallback;

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when PPAPI initialization in the NaCl plugin is finished.
    virtual void StartupInitializationComplete() = 0;

    // Called when irt_open_resource() is invoked in the NaCl plugin.
    // Upon completion, callback is invoked with the file.
    virtual void OpenResource(const std::string& key,
                              OpenResourceCallback callback) = 0;
  };

  ManifestServiceChannel(const IPC::ChannelHandle& handle,
                         base::OnceCallback<void(int32_t)> connected_callback,
                         std::unique_ptr<Delegate> delegate,
                         base::WaitableEvent* waitable_event);

  ManifestServiceChannel(const ManifestServiceChannel&) = delete;
  ManifestServiceChannel& operator=(const ManifestServiceChannel&) = delete;

  ~ManifestServiceChannel() override;

  void Send(IPC::Message* message);

  // Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnChannelConnected(int32_t peer_pid) override;
  void OnChannelError() override;

 private:
  void OnStartupInitializationComplete();
  void OnOpenResource(const std::string& key, IPC::Message* reply);
  void DidOpenResource(IPC::Message* reply,
                       base::File file,
                       uint64_t token_lo,
                       uint64_t token_hi);
  base::OnceCallback<void(int32_t)> connected_callback_;
  std::unique_ptr<Delegate> delegate_;
  std::unique_ptr<IPC::SyncChannel> channel_;

  base::ProcessId peer_pid_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ManifestServiceChannel> weak_ptr_factory_{this};
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_RENDERER_MANIFEST_SERVICE_CHANNEL_H_
