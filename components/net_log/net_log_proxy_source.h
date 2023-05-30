// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NET_LOG_NET_LOG_PROXY_SOURCE_H_
#define COMPONENTS_NET_LOG_NET_LOG_PROXY_SOURCE_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/log/net_log.h"
#include "services/network/public/mojom/net_log.mojom.h"

namespace base {
class SequencedTaskRunner;
}

namespace net_log {

// Implementation of NetLogProxySource mojo interface which observes local
// NetLog events and proxies them over mojo to a remote NetLogProxySink.
// Observing and proxying is only active when notified through
// UpdateCaptureModes() that capturing is active in the remote process.
class NetLogProxySource : public net::NetLog::ThreadSafeObserver,
                          public network::mojom::NetLogProxySource {
 public:
  // When notified through |proxy_source_receiver| that capturing is active,
  // registers a local NetLog observer and sends all NetLog events to
  // |proxy_sink_remote|.
  // The caller is expected to create a new NetLogProxySource if the remote
  // process is restarted.
  NetLogProxySource(
      mojo::PendingReceiver<network::mojom::NetLogProxySource>
          proxy_source_receiver,
      mojo::Remote<network::mojom::NetLogProxySink> proxy_sink_remote);
  ~NetLogProxySource() override;
  NetLogProxySource(const NetLogProxySource&) = delete;
  NetLogProxySource& operator=(const NetLogProxySource&) = delete;

  // Remove NetLog observer and close mojo pipes.
  void ShutDown();

  // NetLog::ThreadSafeObserver:
  void OnAddEntry(const net::NetLogEntry& entry) override;

  // mojom::NetLogProxySource:
  void UpdateCaptureModes(net::NetLogCaptureModeSet modes) override;

 private:
  // Proxy entry to the remote. Must only be called on |task_runner_|.
  void SendNetLogEntry(net::NetLogEventType type,
                       const net::NetLogSource& net_log_source,
                       net::NetLogEventPhase phase,
                       base::TimeTicks time,
                       base::Value::Dict params);

  mojo::Receiver<network::mojom::NetLogProxySource> proxy_source_receiver_;
  mojo::Remote<network::mojom::NetLogProxySink> proxy_sink_remote_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // A WeakPtr to |this|, which is used when posting tasks from the
  // NetLog::ThreadSafeObserver to |task_runner_|. This single WeakPtr instance
  // is used for all tasks as the ThreadSafeObserver may call on any thread, so
  // the weak_factory_ cannot be accessed safely from those threads.
  base::WeakPtr<NetLogProxySource> weak_this_;

  base::WeakPtrFactory<NetLogProxySource> weak_factory_{this};
};

}  // namespace net_log

#endif  // COMPONENTS_NET_LOG_NET_LOG_PROXY_SOURCE_H_
