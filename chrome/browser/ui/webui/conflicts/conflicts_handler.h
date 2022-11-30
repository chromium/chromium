// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONFLICTS_CONFLICTS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CONFLICTS_CONFLICTS_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/conflicts/conflicts_data_fetcher.h"
#include "content/public/browser/web_ui_message_handler.h"

// This class takes care of sending the list of all loaded modules to the
// chrome://conflicts WebUI page when it is requested.
class ConflictsHandler : public content::WebUIMessageHandler {
 public:
  ConflictsHandler();

  ConflictsHandler(const ConflictsHandler&) = delete;
  ConflictsHandler& operator=(const ConflictsHandler&) = delete;

  ~ConflictsHandler() override;

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // Callback for the "requestModuleList" message.
  void HandleRequestModuleList(const base::Value::List& args);

  void OnConflictsDataFetched(base::Value::Dict results);

  // The ID of the callback that will get invoked with the module list.
  std::string module_list_callback_id_;

  // Responsible for fetching the list of modules from the ModuleDatabase, which
  // lives on a different sequence.
  ConflictsDataFetcher::UniquePtr conflicts_data_fetcher_;

  base::WeakPtrFactory<ConflictsHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_CONFLICTS_CONFLICTS_HANDLER_H_
