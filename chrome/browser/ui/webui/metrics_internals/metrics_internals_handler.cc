// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/metrics_internals/metrics_internals_handler.h"

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "components/metrics/debug/metrics_internals_utils.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_service_observer.h"
#include "third_party/abseil-cpp/absl/time/time.h"

namespace {
// Unwraps a CBOR Web Token (CWT) containing an OKP (RFC 8037 section 2 Octet
// Key Pair) into a dictionary to pass to webui.
base::Value::Dict OkpCwtToDict(
    const fcp::confidential_compute::OkpCwt& decoded_key) {
  base::Value::Dict dict;
  if (decoded_key.issued_at.has_value()) {
    dict.Set("issued_at", base::NumberToString(absl::ToUnixMillis(
                              decoded_key.issued_at.value())));
  }
  if (decoded_key.expiration_time.has_value()) {
    dict.Set("expiration_time", base::NumberToString(absl::ToUnixMillis(
                                    decoded_key.expiration_time.value())));
  }
  CHECK(decoded_key.algorithm.has_value());
  dict.Set("algorithm", static_cast<double>(decoded_key.algorithm.value()));
  dict.Set("config_properties",
           base::HexEncodeLower(decoded_key.config_properties));
  dict.Set("access_policy",
           base::HexEncodeLower(decoded_key.access_policy_sha256));
  dict.Set("signature", base::HexEncodeLower(decoded_key.signature));
  CHECK(decoded_key.public_key.has_value());
  const auto& public_key = *decoded_key.public_key;
  dict.Set("key_id", base::HexEncodeLower(public_key.key_id));
  if (public_key.algorithm.has_value()) {
    dict.Set("key_algorithm",
             static_cast<double>(public_key.algorithm.value()));
  }
  if (public_key.curve.has_value()) {
    dict.Set("key_curve", static_cast<double>(public_key.curve.value()));
  }
  base::Value::List key_ops_list;
  for (const auto& op : public_key.key_ops) {
    key_ops_list.Append(static_cast<double>(op));
  }
  dict.Set("key_ops", std::move(key_ops_list));
  dict.Set("key_x", base::HexEncodeLower(public_key.x));
  dict.Set("key_d", base::HexEncodeLower(public_key.d));
  return dict;
}
}  // namespace

MetricsInternalsHandler::MetricsInternalsHandler() {
  if (!ShouldUseMetricsServiceObserver()) {
    uma_log_observer_ = std::make_unique<metrics::MetricsServiceObserver>(
        metrics::MetricsServiceObserver::MetricsServiceType::UMA);
    g_browser_process->metrics_service()->AddLogsObserver(
        uma_log_observer_.get());
  }
}

MetricsInternalsHandler::~MetricsInternalsHandler() {
  if (uma_log_observer_) {
    g_browser_process->metrics_service()->RemoveLogsObserver(
        uma_log_observer_.get());
  }
}

void MetricsInternalsHandler::OnJavascriptAllowed() {
  uma_log_notified_subscription_ = GetUmaObserver()->AddNotifiedCallback(
      base::BindRepeating(&MetricsInternalsHandler::OnUmaLogCreatedOrEvent,
                          weak_ptr_factory_.GetWeakPtr()));
  if (auto* dwa_service =
          g_browser_process->GetMetricsServicesManager()->GetDwaService()) {
    dwa_service_observation_.Observe(dwa_service);
  }
}

void MetricsInternalsHandler::OnJavascriptDisallowed() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  uma_log_notified_subscription_ = {};
  dwa_service_observation_.Reset();
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

bool MetricsInternalsHandler::ShouldUseMetricsServiceObserver() {
  return g_browser_process->metrics_service()->logs_event_observer() != nullptr;
}

metrics::MetricsServiceObserver* MetricsInternalsHandler::GetUmaObserver() {
  return ShouldUseMetricsServiceObserver()
             ? g_browser_process->metrics_service()->logs_event_observer()
             : uma_log_observer_.get();
}

void MetricsInternalsHandler::HandleFetchVariationsSummary(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(
      callback_id, metrics::GetVariationsSummary(
                       g_browser_process->GetMetricsServicesManager()));
}

void MetricsInternalsHandler::HandleFetchStoredSeedInfo(
    variations::VariationsSeedStore::SeedType seed_type,
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  base::OnceCallback<void(base::ValueView)> resolve_js_callback =
      base::BindOnce(&MetricsInternalsHandler::ResolveJavascriptCallback,
                     weak_ptr_factory_.GetWeakPtr(), callback_id.Clone());
  metrics::GetStoredSeedInfo(std::move(resolve_js_callback),
                             g_browser_process->GetMetricsServicesManager(),
                             seed_type);
}

void MetricsInternalsHandler::HandleFetchUmaSummary(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(
      callback_id,
      metrics::GetUmaSummary(
          g_browser_process->GetMetricsServicesManager()->GetMetricsService()));
}

void MetricsInternalsHandler::HandleFetchUmaLogsData(
    const base::Value::List& args) {
  AllowJavascript();
  // |args| should have two elements: the callback ID, and a bool parameter that
  // determines whether we should include log proto data.
  DCHECK_EQ(args.size(), 2U);
  const base::Value& callback_id = args[0];
  const bool include_log_proto_data = args[1].GetBool();

  std::string logs_json;
  bool result =
      GetUmaObserver()->ExportLogsAsJson(include_log_proto_data, &logs_json);
  DCHECK(result);
  ResolveJavascriptCallback(callback_id, base::Value(std::move(logs_json)));
}

void MetricsInternalsHandler::HandleFetchEncryptionPublicKey(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  base::Value::Dict result;
  if (auto* dwa_service =
          g_browser_process->GetMetricsServicesManager()->GetDwaService()) {
    const auto& cwt = dwa_service->GetEncryptionPublicKey();
    if (cwt.has_value()) {
      result = OkpCwtToDict(*cwt);
    }
  }
  ResolveJavascriptCallback(callback_id, base::Value(std::move(result)));
}

void MetricsInternalsHandler::HandleIsUsingMetricsServiceObserver(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id,
                            base::Value(ShouldUseMetricsServiceObserver()));
}

void MetricsInternalsHandler::OnUmaLogCreatedOrEvent() {
  FireWebUIListener("uma-log-created-or-event");
}

void MetricsInternalsHandler::OnEncryptionPublicKeyChanged(
    const fcp::confidential_compute::OkpCwt& decoded_key) {
  FireWebUIListener("encryption-public-key-changed", OkpCwtToDict(decoded_key));
}
