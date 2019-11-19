// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/conflicts/conflicts_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"

ConflictsHandler::ConflictsHandler()
    : conflicts_data_fetcher_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {}

ConflictsHandler::~ConflictsHandler() = default;

void ConflictsHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  web_ui()->RegisterMessageCallback(
      "requestModuleList",
      base::BindRepeating(&ConflictsHandler::HandleRequestModuleList,
                          base::Unretained(this)));
}

void ConflictsHandler::HandleRequestModuleList(const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Make sure the JS doesn't call 'requestModuleList' more than once.
  // TODO(739291): It would be better to kill the renderer instead of the
  // browser for malformed messages.
  CHECK_EQ(1U, args->GetSize());
  CHECK(args->GetString(0, &module_list_callback_id_));

  conflicts_data_fetcher_ = ConflictsDataFetcher::Create(
      base::BindOnce(&ConflictsHandler::OnConflictsDataFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ConflictsHandler::OnConflictsDataFetched(base::DictionaryValue results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!module_list_callback_id_.empty());

  conflicts_data_fetcher_ = nullptr;

  AllowJavascript();
  ResolveJavascriptCallback(base::Value(module_list_callback_id_), results);
}
