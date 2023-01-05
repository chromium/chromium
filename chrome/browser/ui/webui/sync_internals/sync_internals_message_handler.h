// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_SYNC_INTERNALS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_SYNC_INTERNALS_MESSAGE_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/sync/engine/events/protocol_event_observer.h"
#include "components/sync/invalidations/invalidations_listener.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace syncer {
class SyncService;
class SyncInvalidationsService;
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

  // Handler for requestStopKeepData message.
  void HandleRequestStopKeepData(const base::Value::List& args);

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

  void OnGotEntityCounts(
      const std::vector<syncer::TypeEntitiesCount>& entity_counts);

  // Gets the SyncService of the underlying original profile. May return
  // nullptr (e.g. if sync is disabled on the command line).
  syncer::SyncService* GetSyncService();

  // Gets the SyncInvalidationsService of the underlying original profile.
  syncer::SyncInvalidationsService* GetSyncInvalidationsService();

  // Unregisters for notifications from all notifications coming from the sync
  // machinery. Leaves notifications hooked into the UI alone.
  void UnregisterModelNotifications();

  // A flag used to prevent double-registration with SyncService.
  bool is_registered_ = false;

  // Whether specifics should be included when converting protocol events to a
  // human readable format.
  bool include_specifics_ = false;

  // An abstraction of who creates the about sync info value map.
  AboutSyncDataDelegate about_sync_data_delegate_;

  base::WeakPtrFactory<SyncInternalsMessageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_SYNC_INTERNALS_MESSAGE_HANDLER_H_
