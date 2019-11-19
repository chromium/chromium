// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/network_properties_manager.h"

#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/clock.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace data_reduction_proxy {

namespace {

constexpr size_t kMaxCacheSize = 10u;

// Maximum number of times a data saver proxy with unique tuple
// (is_secure_proxy, is_core_proxy) is probed.
constexpr size_t kMaxWarmupURLFetchAttempts = 3;

// Parses a base::Value to NetworkProperties struct. If the parsing is
// unsuccessful, a nullptr is returned.
base::Optional<NetworkProperties> GetParsedNetworkProperty(
    const base::Value& value) {
  if (!value.is_string())
    return base::nullopt;

  std::string base64_decoded;
  if (!base::Base64Decode(value.GetString(), &base64_decoded))
    return base::nullopt;

  NetworkProperties network_properties;
  if (!network_properties.ParseFromString(base64_decoded))
    return base::nullopt;

  return network_properties;
}

}  // namespace

class NetworkPropertiesManager::PrefManager {
 public:
  PrefManager(base::Clock* clock, PrefService* pref_service)
      : clock_(clock), pref_service_(pref_service) {
    DCHECK(clock_);
    DictionaryPrefUpdate update(pref_service_, prefs::kNetworkProperties);
    base::DictionaryValue* properties_dict = update.Get();

    CleanupOldOrInvalidValues(properties_dict);
  }

  ~PrefManager() {}

  void OnChangeInNetworkProperty(const std::string& network_id,
                                 const NetworkProperties& network_properties) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    std::string serialized_network_properties;
    bool serialize_to_string_ok =
        network_properties.SerializeToString(&serialized_network_properties);
    if (!serialize_to_string_ok)
      return;

    std::string base64_encoded;
    base::Base64Encode(serialized_network_properties, &base64_encoded);

    DictionaryPrefUpdate update(pref_service_, prefs::kNetworkProperties);
    base::DictionaryValue* properties_dict = update.Get();
    properties_dict->SetKey(network_id, base::Value(base64_encoded));

    LimitPrefSize(properties_dict);
  }

  void DeleteHistory() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    pref_service_->ClearPref(prefs::kNetworkProperties);
  }

 private:
  // Limits the pref size to kMaxCacheSize.
  void LimitPrefSize(base::DictionaryValue* properties_dict) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (properties_dict->size() <= kMaxCacheSize)
      return;

    // Delete the key that corresponds to the network with the earliest
    // timestamp.
    const std::string* key_to_delete = nullptr;
    int64_t earliest_timestamp = std::numeric_limits<int64_t>::max();

    for (const auto& it : properties_dict->DictItems()) {
      base::Optional<NetworkProperties> network_properties =
          GetParsedNetworkProperty(it.second);
      if (!network_properties) {
        // Delete the corrupted entry. No need to find the oldest entry.
        key_to_delete = &it.first;
        break;
      }

      int64_t timestamp = network_properties.value().last_modified();

      if (timestamp < earliest_timestamp) {
        earliest_timestamp = timestamp;
        key_to_delete = &it.first;
      }
    }
    if (key_to_delete == nullptr)
      return;
    properties_dict->RemoveKey(*key_to_delete);
  }

  // Removes all old or invalid values from the dictionary.
  void CleanupOldOrInvalidValues(base::DictionaryValue* properties_dict) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Entries that have not been modified during last |kMaxEntryAge| would be
    // deleted.
    static constexpr base::TimeDelta kMaxEntryAge =
        base::TimeDelta::FromDays(30);
    const base::Time now = clock_->Now();

    std::vector<std::string> keys_to_delete;

    for (const auto& it : properties_dict->DictItems()) {
      // At most one entry is deleted within this loop since |it| may not
      // be valid once an entry is deleted from the dictionary.
      base::Optional<NetworkProperties> network_properties =
          GetParsedNetworkProperty(it.second);
      if (!network_properties) {
        // Delete the corrupted entry.
        keys_to_delete.push_back(it.first);
        continue;
      }

      base::Time timestamp_entry =
          base::Time::FromJavaTime(network_properties.value().last_modified());
      base::TimeDelta entry_age = now - timestamp_entry;
      if (entry_age < base::TimeDelta() || entry_age > kMaxEntryAge) {
        keys_to_delete.push_back(it.first);
        continue;
      }
    }

    for (const auto& key : keys_to_delete)
      properties_dict->RemoveKey(key);
  }

  base::Clock* clock_;

  // Guaranteed to be non-null during the lifetime of |this|.
  PrefService* pref_service_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PrefManager);
};

NetworkPropertiesManager::NetworkPropertiesManager(base::Clock* clock,
                                                   PrefService* pref_service)
    : clock_(clock),
      network_properties_container_(ConvertDictionaryValueToParsedPrefs(
          pref_service->GetDictionary(prefs::kNetworkProperties))) {
  DCHECK(clock_);

  pref_manager_ = std::make_unique<PrefManager>(clock_, pref_service);

  ResetWarmupURLFetchMetrics();

  DETACH_FROM_SEQUENCE(sequence_checker_);
}

NetworkPropertiesManager::~NetworkPropertiesManager() {
}

void NetworkPropertiesManager::DeleteHistory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_manager_->DeleteHistory();
}

void NetworkPropertiesManager::OnChangeInNetworkID(
    const std::string& network_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!network_id.empty());

  ResetWarmupURLFetchMetrics();

  network_id_ = network_id;

  bool cached_entry_found = false;

  NetworkPropertiesContainer::const_iterator it =
      network_properties_container_.find(network_id_);
  if (it != network_properties_container_.end()) {
    network_properties_ = it->second;
    cached_entry_found = true;
  } else {
    // Reset to default state.
    network_properties_.Clear();
  }
  UMA_HISTOGRAM_BOOLEAN("DataReductionProxy.NetworkProperties.CacheHit",
                        cached_entry_found);
}

void NetworkPropertiesManager::ResetWarmupURLFetchMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  has_warmup_url_succeded_secure_core_ = false;
  has_warmup_url_succeded_secure_non_core_ = false;
  has_warmup_url_succeded_insecure_core_ = false;
  has_warmup_url_succeded_insecure_non_core_ = false;

  warmup_url_fetch_attempt_counts_secure_core_ = 0;
  warmup_url_fetch_attempt_counts_secure_non_core_ = 0;
  warmup_url_fetch_attempt_counts_insecure_core_ = 0;
  warmup_url_fetch_attempt_counts_insecure_non_core_ = 0;
}

void NetworkPropertiesManager::OnChangeInNetworkProperty() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  network_properties_.set_last_modified(clock_->Now().ToJavaTime());
  // Remove the entry from the map, if it is already present.
  network_properties_container_.erase(network_id_);
  network_properties_container_.emplace(
      std::make_pair(network_id_, network_properties_));

  pref_manager_->OnChangeInNetworkProperty(network_id_, network_properties_);
}

// static
NetworkPropertiesManager::NetworkPropertiesContainer
NetworkPropertiesManager::ConvertDictionaryValueToParsedPrefs(
    const base::Value* value) {
  NetworkPropertiesContainer read_prefs;

  for (const auto& it : value->DictItems()) {
    base::Optional<NetworkProperties> network_properties =
        GetParsedNetworkProperty(it.second);
    if (!network_properties)
      continue;
    read_prefs.emplace(std::make_pair(it.first, network_properties.value()));
  }

  return read_prefs;
}

bool NetworkPropertiesManager::IsSecureProxyAllowed(bool is_core_proxy) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_core_proxy);
  return !network_properties_.secure_proxy_disallowed_by_carrier() &&
         !network_properties_.has_captive_portal() &&
         !HasWarmupURLProbeFailed(true, is_core_proxy);
}

bool NetworkPropertiesManager::IsInsecureProxyAllowed(
    bool is_core_proxy) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_core_proxy);
  return !HasWarmupURLProbeFailed(false, is_core_proxy);
}

bool NetworkPropertiesManager::IsSecureProxyDisallowedByCarrier() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return network_properties_.secure_proxy_disallowed_by_carrier();
}

void NetworkPropertiesManager::SetIsSecureProxyDisallowedByCarrier(
    bool disallowed_by_carrier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network_properties_.set_secure_proxy_disallowed_by_carrier(
      disallowed_by_carrier);
  OnChangeInNetworkProperty();
}

bool NetworkPropertiesManager::IsCaptivePortal() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return network_properties_.has_captive_portal();
}

void NetworkPropertiesManager::SetIsCaptivePortal(bool is_captive_portal) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network_properties_.set_has_captive_portal(is_captive_portal);
  OnChangeInNetworkProperty();
}

bool NetworkPropertiesManager::HasWarmupURLProbeFailed(
    bool secure_proxy,
    bool is_core_proxy) const {
  DCHECK(is_core_proxy);
  if (secure_proxy && is_core_proxy) {
    return network_properties_.secure_proxy_flags()
        .disallowed_due_to_warmup_probe_failure();
  }
  if (secure_proxy && !is_core_proxy) {
    return network_properties_.secure_non_core_proxy_flags()
        .disallowed_due_to_warmup_probe_failure();
  }
  if (!secure_proxy && is_core_proxy) {
    return network_properties_.insecure_proxy_flags()
        .disallowed_due_to_warmup_probe_failure();
  }
  if (!secure_proxy && !is_core_proxy) {
    return network_properties_.insecure_non_core_proxy_flags()
        .disallowed_due_to_warmup_probe_failure();
  }
  NOTREACHED();
  return false;
}

void NetworkPropertiesManager::SetHasWarmupURLProbeFailed(
    bool secure_proxy,
    bool is_core_proxy,
    bool warmup_url_probe_failed) {
  DCHECK(is_core_proxy);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (secure_proxy && is_core_proxy) {
    if (!warmup_url_probe_failed) {
      UMA_HISTOGRAM_EXACT_LINEAR(
          "DataReductionProxy.WarmupURL.FetchAttemptsBeforeSuccess.Secure.Core",
          warmup_url_fetch_attempt_counts_secure_core_, 10);
    }
    has_warmup_url_succeded_secure_core_ = !warmup_url_probe_failed;
    network_properties_.mutable_secure_proxy_flags()
        ->set_disallowed_due_to_warmup_probe_failure(warmup_url_probe_failed);
  } else if (secure_proxy && !is_core_proxy) {
    if (!warmup_url_probe_failed) {
      UMA_HISTOGRAM_EXACT_LINEAR(
          "DataReductionProxy.WarmupURL.FetchAttemptsBeforeSuccess.Secure."
          "NonCore",
          warmup_url_fetch_attempt_counts_secure_non_core_, 10);
    }
    has_warmup_url_succeded_secure_non_core_ = !warmup_url_probe_failed;
    network_properties_.mutable_secure_non_core_proxy_flags()
        ->set_disallowed_due_to_warmup_probe_failure(warmup_url_probe_failed);
  } else if (!secure_proxy && is_core_proxy) {
    if (!warmup_url_probe_failed) {
      UMA_HISTOGRAM_EXACT_LINEAR(
          "DataReductionProxy.WarmupURL.FetchAttemptsBeforeSuccess.Insecure."
          "Core",
          warmup_url_fetch_attempt_counts_insecure_core_, 10);
    }
    has_warmup_url_succeded_insecure_core_ = !warmup_url_probe_failed;
    network_properties_.mutable_insecure_proxy_flags()
        ->set_disallowed_due_to_warmup_probe_failure(warmup_url_probe_failed);
  } else {
    if (!warmup_url_probe_failed) {
      UMA_HISTOGRAM_EXACT_LINEAR(
          "DataReductionProxy.WarmupURL.FetchAttemptsBeforeSuccess.Insecure."
          "NonCore",
          warmup_url_fetch_attempt_counts_insecure_non_core_, 10);
    }
    has_warmup_url_succeded_insecure_non_core_ = !warmup_url_probe_failed;
    network_properties_.mutable_insecure_non_core_proxy_flags()
        ->set_disallowed_due_to_warmup_probe_failure(warmup_url_probe_failed);
  }
  OnChangeInNetworkProperty();
}

bool NetworkPropertiesManager::ShouldFetchWarmupProbeURL(
    bool secure_proxy,
    bool is_core_proxy) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (secure_proxy && is_core_proxy) {
    return !has_warmup_url_succeded_secure_core_ &&
           warmup_url_fetch_attempt_counts_secure_core_ <
               kMaxWarmupURLFetchAttempts;
  } else if (secure_proxy && !is_core_proxy) {
    return !has_warmup_url_succeded_secure_non_core_ &&
           warmup_url_fetch_attempt_counts_secure_non_core_ <
               kMaxWarmupURLFetchAttempts;
  } else if (!secure_proxy && is_core_proxy) {
    return !has_warmup_url_succeded_insecure_core_ &&
           warmup_url_fetch_attempt_counts_insecure_core_ <
               kMaxWarmupURLFetchAttempts;
  } else {
    return !has_warmup_url_succeded_insecure_non_core_ &&
           warmup_url_fetch_attempt_counts_insecure_non_core_ <
               kMaxWarmupURLFetchAttempts;
  }
}

void NetworkPropertiesManager::OnWarmupFetchInitiated(bool secure_proxy,
                                                      bool is_core_proxy) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (secure_proxy && is_core_proxy) {
    ++warmup_url_fetch_attempt_counts_secure_core_;
    DCHECK_GE(kMaxWarmupURLFetchAttempts,
              warmup_url_fetch_attempt_counts_secure_core_);
  } else if (secure_proxy && !is_core_proxy) {
    ++warmup_url_fetch_attempt_counts_secure_non_core_;
    DCHECK_GE(kMaxWarmupURLFetchAttempts,
              warmup_url_fetch_attempt_counts_secure_non_core_);
  } else if (!secure_proxy && is_core_proxy) {
    ++warmup_url_fetch_attempt_counts_insecure_core_;
    DCHECK_GE(kMaxWarmupURLFetchAttempts,
              warmup_url_fetch_attempt_counts_insecure_core_);
  } else {
    ++warmup_url_fetch_attempt_counts_insecure_non_core_;
    DCHECK_GE(kMaxWarmupURLFetchAttempts,
              warmup_url_fetch_attempt_counts_insecure_non_core_);
  }
}

size_t NetworkPropertiesManager::GetWarmupURLFetchAttemptCounts(
    bool secure_proxy,
    bool is_core_proxy) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (secure_proxy && is_core_proxy) {
    DCHECK_GT(kMaxWarmupURLFetchAttempts,
              warmup_url_fetch_attempt_counts_secure_core_);
    return warmup_url_fetch_attempt_counts_secure_core_;
  } else if (secure_proxy && !is_core_proxy) {
    DCHECK_GT(kMaxWarmupURLFetchAttempts,
              warmup_url_fetch_attempt_counts_secure_non_core_);
    return warmup_url_fetch_attempt_counts_secure_non_core_;
  } else if (!secure_proxy && is_core_proxy) {
    DCHECK_GT(kMaxWarmupURLFetchAttempts,
              warmup_url_fetch_attempt_counts_insecure_core_);
    return warmup_url_fetch_attempt_counts_insecure_core_;
  } else {
    DCHECK_GT(kMaxWarmupURLFetchAttempts,
              warmup_url_fetch_attempt_counts_insecure_non_core_);
    return warmup_url_fetch_attempt_counts_insecure_non_core_;
  }
}

}  // namespace data_reduction_proxy
