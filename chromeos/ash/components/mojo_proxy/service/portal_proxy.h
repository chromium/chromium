// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_SERVICE_PORTAL_PROXY_H_
#define CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_SERVICE_PORTAL_PROXY_H_

#include <cstdint>

#include "base/memory/raw_ref.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/c/system/trap.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/c/system/types.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/cpp/system/message_pipe.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/cpp/system/trap.h"
#include "mojo/core/scoped_ipcz_handle.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo_proxy {

class NodeProxy;

// Maintains a proxy between a single ipcz portal and a corresponding legacy
// Mojo message pipe. Self-destructs when either end is disconnected.
class PortalProxy {
 public:
  PortalProxy(const raw_ref<const IpczAPI> ipcz,
              NodeProxy& node_proxy,
              mojo::core::ScopedIpczHandle portal,
              mojo_legacy::ScopedMessagePipeHandle pipe);
  ~PortalProxy();

  // Starts proxying. Until either the portal or the pipe is disconnected from
  // its peer, this will watch both objects for incoming messages and forward
  // them to the other.
  void Start();

 private:
  uintptr_t trap_context() const { return reinterpret_cast<uintptr_t>(this); }

  void Flush();
  void FlushAndWatchPortal();
  void FlushAndWatchPipe();
  mojo::core::ScopedIpczHandle TranslateMojoToIpczHandle(
      mojo_legacy::ScopedHandle handle);
  mojo_legacy::ScopedHandle TranslateIpczToMojoHandle(
      mojo::core::ScopedIpczHandle handle);

  static void OnIpczPortalActivity(const IpczTrapEvent* event) {
    reinterpret_cast<PortalProxy*>(event->context)
        ->HandlePortalActivity(event->condition_flags);
  }

  static void OnMojoPipeActivity(const mojo_legacy::MojoTrapEvent* event) {
    reinterpret_cast<PortalProxy*>(event->trigger_context)
        ->HandlePipeActivity(event->result);
  }

  void HandlePortalActivity(IpczTrapConditionFlags flags);
  void HandlePipeActivity(mojo_legacy::MojoResult result);
  void Die();

  bool in_flush_ = false;
  bool disconnected_ = false;
  bool watching_portal_ = false;
  bool watching_pipe_ = false;

  const raw_ref<const IpczAPI> ipcz_;
  raw_ref<NodeProxy> node_proxy_;
  const mojo::core::ScopedIpczHandle portal_;
  const mojo_legacy::ScopedMessagePipeHandle pipe_;
  mojo_legacy::ScopedTrapHandle pipe_trap_;
};

}  // namespace mojo_proxy

#endif  // CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_SERVICE_PORTAL_PROXY_H_
