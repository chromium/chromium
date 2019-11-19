// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_MESSAGE_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/sync/engine/cycle/type_debug_info_observer.h"
#include "components/sync/engine/events/protocol_event_observer.h"
#include "components/sync/js/js_controller.h"
#include "components/sync/js/js_event_handler.h"
#include "components/version_info/channel.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace syncer {
class SyncService;
}  //  namespace syncer

// The implementation for the chrome://sync-internals page.
class SyncInternalsMessageHandler : public content::WebUIMessageHandler,
                                    public syncer::JsEventHandler,
                                    public syncer::SyncServiceObserver,
                                    public syncer::ProtocolEventObserver,
                                    public syncer::TypeDebugInfoObserver {
 public:
  SyncInternalsMessageHandler();
  ~SyncInternalsMessageHandler() override;

  // content::WebUIMessageHandler implementation.
  void OnJavascriptDisallowed() override;
  void RegisterMessages() override;

  // Sets up observers to receive events and forward them to the UI.
  void HandleRegisterForEvents(const base::ListValue* args);

  // Sets up observers to receive per-type counters and forward them to the UI.
  void HandleRegisterForPerTypeCounters(const base::ListValue* args);

  // Fires an event to send updated info back to the page.
  void HandleRequestUpdatedAboutInfo(const base::ListValue* args);

  // Fires an event to send the list of types back to the page.
  void HandleRequestListOfTypes(const base::ListValue* args);

  // Fires an event to send the initial state of the "include specifics" flag.
  void HandleRequestIncludeSpecificsInitialState(const base::ListValue* args);

  // Handler for getAllNodes message.  Needs a |request_id| argument.
  void HandleGetAllNodes(const base::ListValue* args);

  // Handler for requests to get UserEvents tab visibility.
  void HandleRequestUserEventsVisibility(const base::ListValue* args);

  // Handler for setting internal state of if specifics should be included in
  // protocol events when sent to be displayed.
  void HandleSetIncludeSpecifics(const base::ListValue* args);

  // Handler for writeUserEvent message.
  void HandleWriteUserEvent(const base::ListValue* args);

  // Handler for requestStart message.
  void HandleRequestStart(const base::ListValue* args);

  // Handler for requestStopKeepData message.
  void HandleRequestStopKeepData(const base::ListValue* args);

  // Handler for requestStopClearData message.
  void HandleRequestStopClearData(const base::ListValue* args);

  // Handler for triggerRefresh message.
  void HandleTriggerRefresh(const base::ListValue* args);

  // syncer::JsEventHandler implementation.
  void HandleJsEvent(const std::string& name,
                     const syncer::JsEventDetails& details) override;

  // Callback used in GetAllNodes.
  void OnReceivedAllNodes(int request_id,
                          std::unique_ptr<base::ListValue> nodes);

  // syncer::SyncServiceObserver implementation.
  void OnStateChanged(syncer::SyncService* sync) override;

  // syncer::ProtocolEventObserver implementation.
  void OnProtocolEvent(const syncer::ProtocolEvent& e) override;

  // syncer::TypeDebugInfoObserver implementation.
  void OnCommitCountersUpdated(syncer::ModelType type,
                               const syncer::CommitCounters& counters) override;
  void OnUpdateCountersUpdated(syncer::ModelType type,
                               const syncer::UpdateCounters& counters) override;
  void OnStatusCountersUpdated(syncer::ModelType type,
                               const syncer::StatusCounters& counters) override;

  // Helper to emit counter updates.
  //
  // Used in implementation of On*CounterUpdated methods.  Emits the given
  // dictionary value with additional data to specify the model type and
  // counter type.
  void EmitCounterUpdate(syncer::ModelType type,
                         const std::string& counter_type,
                         std::unique_ptr<base::DictionaryValue> value);

 protected:
  using AboutSyncDataDelegate =
      base::RepeatingCallback<std::unique_ptr<base::DictionaryValue>(
          syncer::SyncService* service,
          version_info::Channel channel)>;

  // Constructor used for unit testing to override dependencies.
  explicit SyncInternalsMessageHandler(
      AboutSyncDataDelegate about_sync_data_delegate);

 private:
  // Fetches updated aboutInfo and sends it to the page in the form of an
  // onAboutInfoUpdated event.
  void SendAboutInfo();

  // Gets the ProfileSyncService of the underlying original profile. May return
  // nullptr (e.g., if sync is disabled on the command line).
  syncer::SyncService* GetSyncService();

  // Sends a dispatch event to the UI. Javascript must be enabled.
  void DispatchEvent(const std::string& name, const base::Value& details_value);

  // Unregisters for notifications from all notifications coming from the sync
  // machinery. Leaves notifications hooked into the UI alone.
  void UnregisterModelNotifications();

  base::WeakPtr<syncer::JsController> js_controller_;

  // A flag used to prevent double-registration with ProfileSyncService.
  bool is_registered_ = false;

  // A flag used to prevent double-registration as TypeDebugInfoObserver with
  // ProfileSyncService.
  bool is_registered_for_counters_ = false;

  // Whether specifics should be included when converting protocol events to a
  // human readable format.
  bool include_specifics_ = false;

  // An abstraction of who creates the about sync info value map.
  AboutSyncDataDelegate about_sync_data_delegate_;

  base::WeakPtrFactory<SyncInternalsMessageHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SyncInternalsMessageHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_MESSAGE_HANDLER_H_
