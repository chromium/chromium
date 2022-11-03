// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_FILE_SYSTEM_INTERNALS_DUMP_DATABASE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_FILE_SYSTEM_INTERNALS_DUMP_DATABASE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

namespace syncfs_internals {

class DumpDatabaseHandler : public content::WebUIMessageHandler {
 public:
  explicit DumpDatabaseHandler(Profile* profile);

  DumpDatabaseHandler(const DumpDatabaseHandler&) = delete;
  DumpDatabaseHandler& operator=(const DumpDatabaseHandler&) = delete;

  ~DumpDatabaseHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  void HandleGetDatabaseDump(const base::Value::List& args);
  void DidGetDatabaseDump(std::string callback_id, base::Value::List list);

  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<DumpDatabaseHandler> weak_factory_{this};
};

}  // namespace syncfs_internals

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_FILE_SYSTEM_INTERNALS_DUMP_DATABASE_HANDLER_H_
