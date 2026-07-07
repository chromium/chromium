// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/debug/metrics_internals_handler_base.h"

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/metrics/debug/metrics_internals_utils.h"
#include "components/metrics/metrics_service.h"
#include "third_party/abseil-cpp/absl/time/time.h"

namespace metrics {

namespace {
// Unwraps a CBOR Web Token (CWT) containing an OKP (RFC 8037 section 2 Octet
// Key Pair) into a dictionary to pass to webui.
base::DictValue OkpCwtToDict(
    const fcp::confidential_compute::OkpCwt& decoded_key) {
  base::DictValue dict;
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
  base::ListValue key_ops_list;
  for (const auto& op : public_key.key_ops) {
    key_ops_list.Append(static_cast<double>(op));
  }
  dict.Set("key_ops", std::move(key_ops_list));
  dict.Set("key_x", base::HexEncodeLower(public_key.x));
  dict.Set("key_d", base::HexEncodeLower(public_key.d));
  return dict;
}
}  // namespace

MetricsInternalsHandlerBase::MetricsInternalsHandlerBase(
    Delegate* delegate,
    MetricsService* metrics_service,
    metrics_services_manager::MetricsServicesManager* metrics_services_manager)
    : delegate_(delegate),
      metrics_service_(metrics_service),
      metrics_services_manager_(metrics_services_manager) {
  if (metrics_service_) {
    if (!ShouldUseMetricsServiceObserver()) {
      uma_log_observer_ = std::make_unique<MetricsServiceObserver>(
          MetricsServiceObserver::MetricsServiceType::UMA);
      metrics_service_->AddLogsObserver(uma_log_observer_.get());
    }
  }
}

MetricsInternalsHandlerBase::~MetricsInternalsHandlerBase() {
  StopObserving();
  if (uma_log_observer_ && metrics_service_) {
    metrics_service_->RemoveLogsObserver(uma_log_observer_.get());
  }
}

void MetricsInternalsHandlerBase::StartObserving() {
  if (metrics_service_ && !uma_log_notified_subscription_) {
    uma_log_notified_subscription_ =
        GetUmaObserver()->AddNotifiedCallback(base::BindRepeating(
            &MetricsInternalsHandlerBase::OnUmaLogCreatedOrEvent,
            weak_ptr_factory_.GetWeakPtr()));
  }
  if (metrics_services_manager_ && !dwa_service_observation_.IsObserving()) {
    if (auto* dwa_service = metrics_services_manager_->GetDwaService()) {
      dwa_service_observation_.Observe(dwa_service);
    }
  }
}

void MetricsInternalsHandlerBase::StopObserving() {
  uma_log_notified_subscription_ = base::CallbackListSubscription();
  dwa_service_observation_.Reset();
}

bool MetricsInternalsHandlerBase::ShouldUseMetricsServiceObserver() {
  return metrics_service_ && metrics_service_->logs_event_observer() != nullptr;
}

MetricsServiceObserver* MetricsInternalsHandlerBase::GetUmaObserver() {
  return ShouldUseMetricsServiceObserver()
             ? metrics_service_->logs_event_observer()
             : uma_log_observer_.get();
}

void MetricsInternalsHandlerBase::HandleFetchVariationsSummary(
    const base::Value& callback_id) {
  delegate_->ResolvePageCallback(
      callback_id, GetVariationsSummary(metrics_services_manager_));
}

void MetricsInternalsHandlerBase::HandleFetchStoredSeedInfo(
    variations::VariationsSeedStore::SeedType seed_type,
    const base::Value& callback_id) {
  base::OnceCallback<void(base::ValueView)> resolve_js_callback =
      base::BindOnce(&MetricsInternalsHandlerBase::ResolveJsCallbackHelper,
                     weak_ptr_factory_.GetWeakPtr(), callback_id.Clone());
  GetStoredSeedInfo(std::move(resolve_js_callback), metrics_services_manager_,
                    seed_type);
}

void MetricsInternalsHandlerBase::ResolveJsCallbackHelper(
    base::Value callback_id,
    base::ValueView result) {
  delegate_->ResolvePageCallback(callback_id, result);
}

void MetricsInternalsHandlerBase::HandleFetchUmaSummary(
    const base::Value& callback_id) {
  delegate_->ResolvePageCallback(callback_id, GetUmaSummary(metrics_service_));
}

void MetricsInternalsHandlerBase::HandleFetchUmaLogsData(
    const base::Value& callback_id,
    bool include_log_proto_data) {
  std::string logs_json;
  auto* observer = GetUmaObserver();
  if (observer) {
    bool result =
        observer->ExportLogsAsJson(include_log_proto_data, &logs_json);
    DCHECK(result);
  }
  delegate_->ResolvePageCallback(callback_id,
                                 base::Value(std::move(logs_json)));
}

void MetricsInternalsHandlerBase::HandleFetchEncryptionPublicKey(
    const base::Value& callback_id) {
  base::DictValue result;
  if (metrics_services_manager_) {
    if (auto* dwa_service = metrics_services_manager_->GetDwaService()) {
      const auto& cwt = dwa_service->GetEncryptionPublicKey();
      if (cwt.has_value()) {
        result = OkpCwtToDict(*cwt);
      }
    }
  }
  delegate_->ResolvePageCallback(callback_id, base::Value(std::move(result)));
}

void MetricsInternalsHandlerBase::HandleIsUsingMetricsServiceObserver(
    const base::Value& callback_id) {
  delegate_->ResolvePageCallback(
      callback_id, base::Value(ShouldUseMetricsServiceObserver()));
}

void MetricsInternalsHandlerBase::OnUmaLogCreatedOrEvent() {
  delegate_->FireWebUIListener("uma-log-created-or-event");
}

void MetricsInternalsHandlerBase::OnEncryptionPublicKeyChanged(
    const fcp::confidential_compute::OkpCwt& decoded_key) {
  delegate_->FireWebUIListener("encryption-public-key-changed",
                               OkpCwtToDict(decoded_key));
}

}  // namespace metrics
