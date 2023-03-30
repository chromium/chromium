// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OFFLINE_OFFLINE_INTERNALS_UI_MESSAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OFFLINE_OFFLINE_INTERNALS_UI_MESSAGE_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/offline_pages/core/background/request_coordinator.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_store_types.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace offline_pages {
enum class GetRequestsResult;
}

namespace offline_internals {

// Class acting as a controller of the chrome://offline-internals WebUI.
class OfflineInternalsUIMessageHandler : public content::WebUIMessageHandler {
 public:
  OfflineInternalsUIMessageHandler();

  OfflineInternalsUIMessageHandler(const OfflineInternalsUIMessageHandler&) =
      delete;
  OfflineInternalsUIMessageHandler& operator=(
      const OfflineInternalsUIMessageHandler&) = delete;

  ~OfflineInternalsUIMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

 private:
  // Delete selected list of page ids from the store.
  void HandleDeleteSelectedPages(const base::Value::List& args);

  // Delete selected list of requests from the request queue.
  void HandleDeleteSelectedRequests(const base::Value::List& args);

  // Load Request Queue info.
  void HandleGetRequestQueue(const base::Value::List& args);

  // Load Stored pages info.
  void HandleGetStoredPages(const base::Value::List& args);

  // Set whether to record offline page model events.
  void HandleSetRecordPageModel(const base::Value::List& args);

  // Set whether to record request queue events.
  void HandleSetRecordRequestQueue(const base::Value::List& args);

  // Set whether to record prefetch service events.
  void HandleSetRecordPrefetchService(const base::Value::List& args);

  // Set whether to enable limitless prefetching.
  void HandleSetLimitlessPrefetchingEnabled(const base::Value::List& args);

  // Get whether limitless prefetching is enabled.
  void HandleGetLimitlessPrefetchingEnabled(const base::Value::List& args);

  // Set whether to enable sending the testing header when making
  // GeneratePageBundle requests.
  void HandleSetPrefetchTestingHeader(const base::Value::List& args);

  // Get whether we are sending the testing header for GeneratePageBundle
  // requests.
  void HandleGetPrefetchTestingHeader(const base::Value::List& args);

  // Load all offline services' event logs.
  void HandleGetEventLogs(const base::Value::List& args);

  // Load whether logs are being recorded.
  void HandleGetLoggingState(const base::Value::List& args);

  // Adds a url to the background loader queue.
  void HandleAddToRequestQueue(const base::Value::List& args);

  // Load whether device is currently offline.
  void HandleGetNetworkStatus(const base::Value::List& args);

  // Schedules an NWake signal.
  void HandleScheduleNwake(const base::Value::List& args);

  // Cancels an NWake signal.
  void HandleCancelNwake(const base::Value::List& args);

  // Sends and processes the request to generate page bundle.
  void HandleGeneratePageBundle(const base::Value::List& args);

  // Sends and processes a request to get the info about an operation.
  void HandleGetOperation(const base::Value::List& args);

  // Downloads an archive.
  void HandleDownloadArchive(const base::Value::List& args);

  // Callback for async GetAllPages calls.
  void HandleStoredPagesCallback(
      std::string callback_id,
      const offline_pages::MultipleOfflinePageItemResult& pages);

  // Callback for async GetRequests calls.
  void HandleRequestQueueCallback(
      std::string callback_id,
      std::vector<std::unique_ptr<offline_pages::SavePageRequest>> requests);

  // Callback for DeletePage/DeleteAllPages calls.
  void HandleDeletedPagesCallback(std::string callback_id,
                                  const offline_pages::DeletePageResult result);

  // Callback for DeleteRequest/DeleteAllRequests calls.
  void HandleDeletedRequestsCallback(
      std::string callback_id,
      const offline_pages::MultipleItemStatuses& results);

  // Callback for SavePageLater calls.
  void HandleSavePageLaterCallback(std::string callback_id,
                                   offline_pages::AddRequestResult result);

  // Offline page model to call methods on.
  raw_ptr<offline_pages::OfflinePageModel> offline_page_model_;

  // Request coordinator for background offline actions.
  raw_ptr<offline_pages::RequestCoordinator> request_coordinator_;

  // Factory for creating references in callbacks.
  base::WeakPtrFactory<OfflineInternalsUIMessageHandler> weak_ptr_factory_{
      this};
};

}  // namespace offline_internals

#endif  // CHROME_BROWSER_UI_WEBUI_OFFLINE_OFFLINE_INTERNALS_UI_MESSAGE_HANDLER_H_
