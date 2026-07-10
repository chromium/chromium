// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/metrics_internals/metrics_internals_handler.h"

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"

MetricsInternalsHandler::MetricsInternalsHandler()
    : base_handler_(std::make_unique<metrics::MetricsInternalsHandlerBase>(
          this,
          g_browser_process->metrics_service(),
          g_browser_process->GetMetricsServicesManager())) {}

MetricsInternalsHandler::~MetricsInternalsHandler() = default;

void MetricsInternalsHandler::OnJavascriptAllowed() {
  base_handler_->StartObserving();
}

void MetricsInternalsHandler::OnJavascriptDisallowed() {
  base_handler_->StopObserving();
}

void MetricsInternalsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "fetchVariationsSummary",
      base::BindRepeating(
          &MetricsInternalsHandler::HandleFetchVariationsSummary,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "fetchStoredLatestSeedInfo",
      base::BindRepeating(&MetricsInternalsHandler::HandleFetchStoredSeedInfo,
                          base::Unretained(this),
                          variations::VariationsSeedStore::SeedType::LATEST));
  web_ui()->RegisterMessageCallback(
      "fetchStoredSafeSeedInfo",
      base::BindRepeating(&MetricsInternalsHandler::HandleFetchStoredSeedInfo,
                          base::Unretained(this),
                          variations::VariationsSeedStore::SeedType::SAFE));
  web_ui()->RegisterMessageCallback(
      "fetchUmaSummary",
      base::BindRepeating(&MetricsInternalsHandler::HandleFetchUmaSummary,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "fetchUmaLogsData",
      base::BindRepeating(&MetricsInternalsHandler::HandleFetchUmaLogsData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "isUsingMetricsServiceObserver",
      base::BindRepeating(
          &MetricsInternalsHandler::HandleIsUsingMetricsServiceObserver,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "fetchEncryptionPublicKey",
      base::BindRepeating(
          &MetricsInternalsHandler::HandleFetchEncryptionPublicKey,
          base::Unretained(this)));
}

void MetricsInternalsHandler::ResolvePageCallback(
    const base::ValueView callback_id,
    const base::ValueView response) {
  ResolveJavascriptCallback(callback_id, response);
}

void MetricsInternalsHandler::FireWebUIListener(std::string_view event_name) {
  content::WebUIMessageHandler::FireWebUIListener(event_name);
}

void MetricsInternalsHandler::FireWebUIListener(std::string_view event_name,
                                                const base::ValueView arg1) {
  content::WebUIMessageHandler::FireWebUIListener(event_name, arg1);
}

void MetricsInternalsHandler::HandleFetchVariationsSummary(
    const base::ListValue& args) {
  AllowJavascript();
  base_handler_->HandleFetchVariationsSummary(args[0]);
}

void MetricsInternalsHandler::HandleFetchStoredSeedInfo(
    variations::VariationsSeedStore::SeedType seed_type,
    const base::ListValue& args) {
  AllowJavascript();
  base_handler_->HandleFetchStoredSeedInfo(seed_type, args[0]);
}

void MetricsInternalsHandler::HandleFetchUmaSummary(
    const base::ListValue& args) {
  AllowJavascript();
  base_handler_->HandleFetchUmaSummary(args[0]);
}

void MetricsInternalsHandler::HandleFetchUmaLogsData(
    const base::ListValue& args) {
  AllowJavascript();
  DCHECK_EQ(args.size(), 2U);
  base_handler_->HandleFetchUmaLogsData(args[0], args[1].GetBool());
}

void MetricsInternalsHandler::HandleFetchEncryptionPublicKey(
    const base::ListValue& args) {
  AllowJavascript();
  base_handler_->HandleFetchEncryptionPublicKey(args[0]);
}

void MetricsInternalsHandler::HandleIsUsingMetricsServiceObserver(
    const base::ListValue& args) {
  AllowJavascript();
  base_handler_->HandleIsUsingMetricsServiceObserver(args[0]);
}
