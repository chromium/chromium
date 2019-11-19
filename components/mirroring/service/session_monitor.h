// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_SESSION_MONITOR_H_
#define COMPONENTS_MIRRORING_SERVICE_SESSION_MONITOR_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/mirroring/mojom/session_observer.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace media {
namespace cast {
class CastEnvironment;
class RawEventSubscriberBundle;
}  // namespace cast
}  // namespace media

namespace mirroring {

class WifiStatusMonitor;

// Monitors a single mirroring session's multiple Cast Streaming "subsessions",
// collecting and managing the following information:
//
//   1. WiFi signal strength, SNR, etc. on the connection between sender and
//      receiver.
//   2. Extra "tags": Information about both the sender and receiver, such as
//      software versions, mirroring settings, and network configuration.
//   3. Snapshots of Cast Streaming event logs (frame- and packet-level events
//      that will allow an analysis of the data flows and a diagnosing of any
//      issues occurring in-the-wild). Start/StopStreamingSession() are called
//      to notify this monitor whenever each (of multiple) Cast Streaming
//      sessions starts and ends.
//
// Either during or at the end of a session, AssembleBundlesAndClear() is called
// to re-package all of the information across multiple snapshots together into
// one bundle, whose blobs can then be included in user feedback report uploads.
//
// To avoid unbounded memory use, older data is discarded automatically if too
// much is accumulating.
class COMPONENT_EXPORT(MIRRORING_SERVICE) SessionMonitor {
 public:
  using EventsAndStats =
      std::pair<std::string /* events */, std::string /* stats */>;
  SessionMonitor(
      int max_retention_bytes,
      const net::IPAddress& receiver_address,
      base::Value session_tags,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> loader_factory);

  ~SessionMonitor();

  enum SessionType {
    AUDIO_ONLY,
    VIDEO_ONLY,
    AUDIO_AND_VIDEO,
  };

  // Notifies this monitor that it may now start/stop monitoring Cast Streaming
  // events/stats.
  void StartStreamingSession(
      scoped_refptr<media::cast::CastEnvironment> cast_environment,
      std::unique_ptr<WifiStatusMonitor> wifi_status_monitor,
      SessionType session_type,
      bool is_remoting);
  void StopStreamingSession();

  // Called when error occurs. Only records the first error since last snapshot.
  void OnStreamingError(mojom::SessionError error);

  // Assembles one or more bundles of data, for inclusion in user feedback
  // reports. The snapshot history is cleared each time this method is called,
  // and so no two calls to this method will return the same data. The caller
  // may request that multiple bundles be produced. This is used, for example,
  // to get one bundle that meets the upload size maximum in addition to
  // another. The total data size in bytes is strictly less-than-or-equal to
  // |bundle_sizes|.
  std::vector<EventsAndStats> AssembleBundlesAndClear(
      const std::vector<int32_t>& bundle_sizes);

  // Takes a snapshot of recent Cast Streaming events and statistics.
  void TakeSnapshot();

  std::string GetReceiverBuildVersion() const;

  // Get receiver's friendly name.
  std::string receiver_name() const { return receiver_name_; }

 private:
  // Query the receiver for its current setup and uptime.
  void QueryReceiverSetupInfo();

  // Get Cast Streaming events/stats.
  std::string GetEventLogsAndReset(bool is_audio,
                                   const std::string& extra_data);
  std::unique_ptr<base::Value> GetStatsAndReset(bool is_audio);

  // Assemble the most-recent events+stats snapshots to a bundle of a byte size
  // less than or equal to |max_bytes|.
  EventsAndStats MakeSliceOfSnapshots(int32_t max_bytes);

  const int max_retention_bytes_;  // Maximum number of bytes to keep.

  const net::IPAddress receiver_address_;

  base::Value session_tags_;  // Streaming session-level tags.

  std::string receiver_name_;

  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

  // Monitors the WiFi status if not null.
  std::unique_ptr<WifiStatusMonitor> wifi_status_monitor_;

  std::unique_ptr<media::cast::RawEventSubscriberBundle> event_subscribers_;

  base::RepeatingTimer snapshot_timer_;

  // The time that the current snapshot starts.
  base::Time start_time_;

  // The most recent snapshots, from oldest to newest. The total size of the
  // data stored in this list is bounded by |max_retention_bytes_|.
  base::circular_deque<EventsAndStats> snapshots_;

  // The number of bytes currently stored in |snapshots_|.
  int stored_snapshots_bytes_;

  base::Time error_time_;
  base::Optional<mojom::SessionError> error_;

  base::WeakPtrFactory<SessionMonitor> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SessionMonitor);
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_SESSION_MONITOR_H_
