// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_DISCOVERY_MEDIA_SINK_SERVICE_BASE_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_DISCOVERY_MEDIA_SINK_SERVICE_BASE_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/discovery/media_sink_service_util.h"

namespace media_router {

class MediaRoute;

// Base class for discovering MediaSinks. Responsible for bookkeeping of
// current set of discovered sinks, and notifying observers when there are
// updates.
// In addition, this class maintains a "discovery timer", used for batching
// updates in quick succession. The timer fires when it is assumed that
// discovery has reached a relatively steady state. When the timer fires:
// - The batched updated sink list will be sent back to the Media Router
// extension via |callback|. This back-channel is necessary until all logic
// dependent on MediaSinks are moved out of the extension.
// - Subclasses may record discovered related metrics.
// This class may be created on any thread, but all subsequent methods must be
// invoked on the same thread.
class MediaSinkServiceBase {
 public:
  // Listens for sink updates in MediaSinkServiceBase.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Invoked when the list of discovered sinks changes.
    virtual void OnSinksDiscovered(
        const std::vector<MediaSinkInternal>& sinks) {}

    // Invoked when |sink| is added or updated.
    virtual void OnSinkAddedOrUpdated(const MediaSinkInternal& sink) {}

    // Invoked when |sink| is removed.
    virtual void OnSinkRemoved(const MediaSinkInternal& sink) {}
  };

  // |callback|: Callback to inform the MediaRouter extension of discovered
  // sinks updates. Other uses should implement Observer::OnSinksDiscovered().
  explicit MediaSinkServiceBase(const OnSinksDiscoveredCallback& callback);

  MediaSinkServiceBase(const MediaSinkServiceBase&) = delete;
  MediaSinkServiceBase& operator=(const MediaSinkServiceBase&) = delete;

  virtual ~MediaSinkServiceBase();

  // Adds |observer| to observe |this| for sink updates.
  // Caller is responsible for calling |RemoveObserver| before it is destroyed.
  // Both methods are safe to call on any thread.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Starts a new round of the discovery process as soon as possible.
  virtual void DiscoverSinksNow() {}

  // Adds or updates, or removes a sink.
  // Notifies |observers_| that the sink has been added, updated, or removed.
  // Also invokes |StartTimer()|.
  void AddOrUpdateSink(const MediaSinkInternal& sink);
  void RemoveSink(const MediaSinkInternal& sink);
  void RemoveSinkById(const MediaSink::Id& sink_id);

  const base::flat_map<MediaSink::Id, MediaSinkInternal>& GetSinks() const;

  // These methods return nullptr when no sink is found.
  const MediaSinkInternal* GetSinkById(const MediaSink::Id& sink_id) const;
  const MediaSinkInternal* GetSinkByRoute(const MediaRoute& route) const;

  void SetTimerForTest(std::unique_ptr<base::OneShotTimer> timer);
  void AddSinkForTest(const MediaSinkInternal& sink);

 protected:
  // Called when |discovery_timer_| expires. Informs subclass to report device
  // counts. Also informs Media Router of updated list of discovered sinks.
  // May be overridden by subclass to perform additional operations, such as
  // pruning old sinks.
  virtual void OnDiscoveryComplete();

  // Starts |discovery_timer_| to invoke |OnDiscoveryComplete()|. Subclasses
  // may call this at the start of a round of discovery.
  void StartTimer();

 private:
  friend class MediaSinkServiceBaseTest;
  FRIEND_TEST_ALL_PREFIXES(MediaSinkServiceBaseTest,
                           TestOnDiscoveryComplete_SameSink);
  FRIEND_TEST_ALL_PREFIXES(MediaSinkServiceBaseTest,
                           TestOnDiscoveryComplete_SameSinkDifferentOrders);

  // Overriden by subclass to report device counts.
  virtual void RecordDeviceCounts() {}

  // The current set of discovered sinks keyed by MediaSink ID.
  base::flat_map<MediaSink::Id, MediaSinkInternal> sinks_;

  // Observers to notify when a sink is added, updated, or removed.
  base::ObserverList<Observer>::Unchecked observers_;

  // Timer for recording device counts after a sink list has changed. To ensure
  // the metrics are recorded accurately, a small delay is introduced after a
  // sink list change in order for the discovery process to reach a steady
  // state before the metrics are recorded.
  std::unique_ptr<base::OneShotTimer> discovery_timer_;

  // The following fields exist temporarily for sending back discovered sinks to
  // the Media Router extension.
  // TODO(crbug.com/40561499): Remove once the extension no longer need
  // the sinks.

  // Callback to MediaRouter to provide sinks to the MR extension.
  OnSinksDiscoveredCallback on_sinks_discovered_cb_;

  // Sinks saved in the previous |OnDiscoveryComplete()| invocation. Checked
  // against |sinks_| during |OnDiscoveryComplete()| before invoking
  // |on_sinks_discovered_cb_|.
  base::flat_map<MediaSink::Id, MediaSinkInternal> previous_sinks_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_DISCOVERY_MEDIA_SINK_SERVICE_BASE_H_
