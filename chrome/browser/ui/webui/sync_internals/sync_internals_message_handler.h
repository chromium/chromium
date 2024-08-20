// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_SYNC_INTERNALS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_SYNC_INTERNALS_MESSAGE_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "base/values.h"
#include "components/sync/engine/events/protocol_event_observer.h"
#include "components/sync/invalidations/invalidations_listener.h"
#include "components/sync/invalidations/sync_invalidations_service.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace syncer {
struct TypeEntitiesCount;
}  //  namespace syncer

// The implementation for the chrome://sync-internals page.
class SyncInternalsMessageHandler : public content::WebUIMessageHandler,
                                    public syncer::SyncServiceObserver,
                                    public syncer::ProtocolEventObserver,
                                    public syncer::InvalidationsListener {
 public:
  SyncInternalsMessageHandler();

  SyncInternalsMessageHandler(const SyncInternalsMessageHandler&) = delete;
  SyncInternalsMessageHandler& operator=(const SyncInternalsMessageHandler&) =
      delete;

  ~SyncInternalsMessageHandler() override;

  // content::WebUIMessageHandler implementation.
  void OnJavascriptDisallowed() override;
  void RegisterMessages() override;

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

  // Handler for requestStopClearData message.
  void HandleRequestStopClearData(const base::Value::List& args);

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

 protected:
  using AboutSyncDataDelegate =
      base::RepeatingCallback<base::Value::Dict(syncer::SyncService* service,
                                                const std::string& channel)>;

  // Constructor used for unit testing to override dependencies.
  explicit SyncInternalsMessageHandler(
      AboutSyncDataDelegate about_sync_data_delegate);

 private:
  // Synchronously fetches updated aboutInfo and sends it to the page in the
  // form of an onAboutInfoUpdated event. The entity counts for each data type
  // are retrieved asynchronously and sent via an onEntityCountsUpdated event
  // once they are retrieved.
  void SendAboutInfoAndEntityCounts();

  void OnGotEntityCounts(const syncer::TypeEntitiesCount& entity_counts);

  // Gets the SyncService of the underlying original profile. May return
  // nullptr (e.g. if sync is disabled on the command line).
  syncer::SyncService* GetSyncService();

  // Gets the SyncInvalidationsService of the underlying original profile.
  syncer::SyncInvalidationsService* GetSyncInvalidationsService();

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

  // An abstraction of who creates the about sync info value map.
  AboutSyncDataDelegate about_sync_data_delegate_;

  base::WeakPtrFactory<SyncInternalsMessageHandler> weak_ptr_factory_{this};
};

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

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_SYNC_INTERNALS_MESSAGE_HANDLER_H_
