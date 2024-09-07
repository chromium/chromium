// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_SYNC_INTERNALS_MESSAGE_HANDLER_H_
#define COMPONENTS_BROWSER_SYNC_SYNC_INTERNALS_MESSAGE_HANDLER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "base/values.h"
#include "components/sync/engine/events/protocol_event_observer.h"
#include "components/sync/invalidations/invalidations_listener.h"
#include "components/sync/invalidations/sync_invalidations_service.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class UserEventService;
struct TypeEntitiesCount;
}  //  namespace syncer

namespace browser_sync {

// Class with the common logic for the chrome://sync-internals page. See also
// ChromeSyncInternalsMessageHandler and IOSSyncInternalsMessageHandler.
class SyncInternalsMessageHandler : public syncer::SyncServiceObserver,
                                    public syncer::ProtocolEventObserver,
                                    public syncer::InvalidationsListener {
 public:
  // Interface that abstracts the platform-specific bits, namely interactions
  // with the page.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Sends `event_name` to the page. The page can listen to events via
    // addWebUiListener().
    virtual void SendEventToPage(std::string_view event_name,
                                 base::span<const base::ValueView> args) = 0;

    // A page can send a message to this class which includes a `callback_id`.
    // This method is then be used to reply to the page with `response`.
    virtual void ResolvePageCallback(const base::ValueView callback_id,
                                     const base::ValueView response) = 0;
  };

  // Handler for a message from the page.
  using PageMessageHandler =
      base::RepeatingCallback<void(const base::Value::List&)>;

  using GetAboutSyncDataCb =
      base::RepeatingCallback<base::Value::Dict(syncer::SyncService* service,
                                                const std::string& channel)>;

  // `delegate` must be non-null and outlive this object.
  SyncInternalsMessageHandler(
      Delegate* delegate,
      signin::IdentityManager* identity_manager,
      syncer::SyncService* sync_service,
      syncer::SyncInvalidationsService* sync_invalidations_service,
      syncer::UserEventService* user_event_service,
      const std::string& channel);

  // Constructor used for unit testing to override dependencies.
  SyncInternalsMessageHandler(
      Delegate* delegate,
      GetAboutSyncDataCb get_about_sync_data_cb,
      signin::IdentityManager* identity_manager,
      syncer::SyncService* sync_service,
      syncer::SyncInvalidationsService* sync_invalidations_service,
      syncer::UserEventService* user_event_service,
      const std::string& channel);

  SyncInternalsMessageHandler(const SyncInternalsMessageHandler&) = delete;
  SyncInternalsMessageHandler& operator=(const SyncInternalsMessageHandler&) =
      delete;

  ~SyncInternalsMessageHandler() override;

  // Returns the messages handled by this class and the corresponding handler.
  // Such messages can be sent by the page via chrome.send(). The embedder is
  // responsible for actually registering the handlers, since the mechanism
  // depends on the platform.
  base::flat_map<std::string, PageMessageHandler> GetMessageHandlerMap();

  // Disables all messages sent from this class to the page via
  // SendEventToPage() or ResolvePageCallback().
  void DisableMessagesToPage();

 private:
  // Synchronously fetches updated aboutInfo and sends it to the page in the
  // form of an onAboutInfoUpdated event. The entity counts for each data type
  // are retrieved asynchronously and sent via an onEntityCountsUpdated event
  // once they are retrieved.
  void SendAboutInfoAndEntityCounts();

  void OnGotEntityCounts(const syncer::TypeEntitiesCount& entity_counts);

  // Fires an event to send updated data to the About page and registers
  // observers to notify the page upon updates.
  void HandleRequestDataAndRegisterForUpdates(const base::Value::List& args);

  // Fires an event to send the list of types back to the page.
  void HandleRequestListOfTypes(const base::Value::List& args);

  // Fires an event to send the initial state of the "include specifics" flag.
  void HandleRequestIncludeSpecificsInitialState(const base::Value::List& args);

  // Handler for getAllNodes message.  Needs a |request_id| argument.
  void HandleGetAllNodes(const base::Value::List& args);

  // Handler for setting internal state of if specifics should be included in
  // protocol events when sent to be displayed.
  void HandleSetIncludeSpecifics(const base::Value::List& args);

  // Handler for writeUserEvent message.
  void HandleWriteUserEvent(const base::Value::List& args);

  // Handler for requestStart message.
  void HandleRequestStart(const base::Value::List& args);

  // Handler for triggerRefresh message.
  void HandleTriggerRefresh(const base::Value::List& args);

  // Callback used in GetAllNodes.
  void OnReceivedAllNodes(const std::string& callback_id,
                          base::Value::List nodes);

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

  // syncer::ProtocolEventObserver implementation.
  void OnProtocolEvent(const syncer::ProtocolEvent& e) override;

  // syncer::InvalidationsListener implementation.
  void OnInvalidationReceived(const std::string& payload) override;

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
  base::ScopedObservation<syncer::SyncService, syncer::ProtocolEventObserver>
      protocol_event_observation_{this};
  base::ScopedObservation<syncer::SyncInvalidationsService,
                          syncer::InvalidationsListener>
      invalidations_observation_{this};

  // Whether specifics should be included when converting protocol events to a
  // human readable format.
  bool include_specifics_ = false;

  const raw_ptr<Delegate> delegate_;
  // An abstraction of who creates the about sync info value map.
  const GetAboutSyncDataCb get_about_sync_data_cb_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<syncer::SyncService> sync_service_;
  const raw_ptr<syncer::SyncInvalidationsService> sync_invalidations_service_;
  const raw_ptr<syncer::UserEventService> user_event_service_;
  const std::string channel_;

  base::WeakPtrFactory<SyncInternalsMessageHandler> weak_ptr_factory_{this};
};

}  // namespace browser_sync

namespace base {

// Required to use base::ScopedObservation with syncer::ProtocolEventObserver,
// since the methods are not called AddObserver/RemoveObserver.
template <>
struct ScopedObservationTraits<syncer::SyncService,
                               syncer::ProtocolEventObserver> {
  static void AddObserver(syncer::SyncService* source,
                          syncer::ProtocolEventObserver* observer) {
    source->AddProtocolEventObserver(observer);
  }
  static void RemoveObserver(syncer::SyncService* source,
                             syncer::ProtocolEventObserver* observer) {
    source->RemoveProtocolEventObserver(observer);
  }
};

// Required to use base::ScopedObservation with syncer::InvalidationsListener,
// since the methods are not called AddObserver/RemoveObserver.
template <>
struct ScopedObservationTraits<syncer::SyncInvalidationsService,
                               syncer::InvalidationsListener> {
  static void AddObserver(syncer::SyncInvalidationsService* source,
                          syncer::InvalidationsListener* observer) {
    source->AddListener(observer);
  }
  static void RemoveObserver(syncer::SyncInvalidationsService* source,
                             syncer::InvalidationsListener* observer) {
    source->RemoveListener(observer);
  }
};

}  // namespace base

#endif  // COMPONENTS_BROWSER_SYNC_SYNC_INTERNALS_MESSAGE_HANDLER_H_
