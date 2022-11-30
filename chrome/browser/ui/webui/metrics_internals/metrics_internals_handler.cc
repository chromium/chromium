// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/metrics_internals/metrics_internals_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "components/metrics/metrics_service.h"

void MetricsInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "fetchClientId",
      base::BindRepeating(&MetricsInternalsHandler::HandleFetchClientId,
                          base::Unretained(this)));
}

void MetricsInternalsHandler::HandleFetchClientId(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(
      callback_id,
      base::Value(g_browser_process->metrics_service()->GetClientId()));
}
