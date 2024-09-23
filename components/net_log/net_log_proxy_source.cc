// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/net_log/net_log_proxy_source.h"
#include "base/task/sequenced_task_runner.h"

namespace net_log {

NetLogProxySource::NetLogProxySource(
    mojo::PendingReceiver<network::mojom::NetLogProxySource>
        proxy_source_receiver,
    mojo::Remote<network::mojom::NetLogProxySink> proxy_sink_remote)
    : proxy_source_receiver_(this, std::move(proxy_source_receiver)),
      proxy_sink_remote_(std::move(proxy_sink_remote)),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  // Initialize a WeakPtr instance that can be safely referred to from other
  // threads when binding tasks posted back to this thread.
  weak_this_ = weak_factory_.GetWeakPtr();
}

NetLogProxySource::~NetLogProxySource() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // Just in case ShutDown() was not called, make sure observer is removed.
  UpdateCaptureModes(0);
}

void NetLogProxySource::ShutDown() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  UpdateCaptureModes(0);
  weak_factory_.InvalidateWeakPtrs();
  proxy_source_receiver_.reset();
  proxy_sink_remote_.reset();
}

void NetLogProxySource::OnAddEntry(const net::NetLogEntry& entry) {
  if (task_runner_->RunsTasksInCurrentSequence()) {
    SendNetLogEntry(entry.type, entry.source, entry.phase, entry.time,
                    entry.params.Clone());
  } else {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&NetLogProxySource::SendNetLogEntry, weak_this_,
                       entry.type, entry.source, entry.phase, entry.time,
                       entry.params.Clone()));
  }
}

void NetLogProxySource::UpdateCaptureModes(
    net::NetLogCaptureModeSet new_modes) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Not capturing, remove observer if necessary.
  if (new_modes == 0) {
    if (net_log())
      net::NetLog::Get()->RemoveObserver(this);
    return;
  }

  // NetLog allows multiple observers to be registered at once, and each might
  // have a different capture mode. In the common case there would normally be
  // at most one observer registered. To avoid the complication of sending
  // different sets of params for each capture mode, if multiple capture modes
  // are active, only one observer is registered in the source process, using
  // the lowest common denominator capture mode. This handles the common case,
  // and in the rare case where multiple capture modes active, is a safe
  // fallback.
  //
  // Register observer for the lowest level that is set in |new_modes|.
  for (int i = 0; i <= static_cast<int>(net::NetLogCaptureMode::kLast); ++i) {
    net::NetLogCaptureMode mode = static_cast<net::NetLogCaptureMode>(i);
    if (net::NetLogCaptureModeSetContains(mode, new_modes)) {
      if (net_log() && capture_mode() == mode) {
        // Already listening at the desired level.
        return;
      }
      if (net_log()) {
        // Listening at the wrong level, remove observer.
        net::NetLog::Get()->RemoveObserver(this);
      }

      net::NetLog::Get()->AddObserver(this, mode);
      return;
    }
  }

  NOTREACHED_IN_MIGRATION();
}

void NetLogProxySource::SendNetLogEntry(net::NetLogEventType type,
                                        const net::NetLogSource& net_log_source,
                                        net::NetLogEventPhase phase,
                                        base::TimeTicks time,
                                        base::Value::Dict params) {
  proxy_sink_remote_->AddEntry(static_cast<uint32_t>(type), net_log_source,
                               phase, time, std::move(params));
}

}  // namespace net_log
