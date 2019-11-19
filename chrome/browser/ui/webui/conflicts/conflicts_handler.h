// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONFLICTS_CONFLICTS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CONFLICTS_CONFLICTS_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/conflicts/conflicts_data_fetcher.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class DictionaryValue;
class Listvalue;
}

// This class takes care of sending the list of all loaded modules to the
// chrome://conflicts WebUI page when it is requested.
class ConflictsHandler : public content::WebUIMessageHandler {
 public:
  ConflictsHandler();
  ~ConflictsHandler() override;

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // Callback for the "requestModuleList" message.
  void HandleRequestModuleList(const base::ListValue* args);

  void OnConflictsDataFetched(base::DictionaryValue results);

  // The ID of the callback that will get invoked with the module list.
  std::string module_list_callback_id_;

  // Responsible for fetching the list of modules from the ModuleDatabase, which
  // lives on a different sequence.
  ConflictsDataFetcher::UniquePtr conflicts_data_fetcher_;

  base::WeakPtrFactory<ConflictsHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ConflictsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CONFLICTS_CONFLICTS_HANDLER_H_
