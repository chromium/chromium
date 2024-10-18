// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_breakage_exception.h"

#include <string>
#include <utility>

#include "components/fingerprinting_protection_filter/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace fingerprinting_protection_filter {

std::string GetEtldPlusOne(const GURL& url) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

bool AddBreakageException(const GURL& url, PrefService& pref_service) {
  const std::string exception_dict_key = GetEtldPlusOne(url);
  if (exception_dict_key.empty()) {
    return false;
  }
  if (HasBreakageException(url, pref_service)) {
    // If exception already exists, do nothing to save processing time of
    // reserializing pref value, and return true since an exception exists.
    return true;
  }
  base::Value::Dict exception_dict =
      pref_service.GetDict(prefs::kRefreshHeuristicBreakageException).Clone();
  exception_dict.Set(exception_dict_key, true);
  pref_service.SetDict(prefs::kRefreshHeuristicBreakageException,
                       std::move(exception_dict));
  return true;
}

bool HasBreakageException(const GURL& url, const PrefService& pref_service) {
  const std::string exception_dict_key = GetEtldPlusOne(url);
  if (exception_dict_key.empty()) {
    return false;
  }
  return pref_service.GetDict(prefs::kRefreshHeuristicBreakageException)
      .contains(exception_dict_key);
}

}  // namespace fingerprinting_protection_filter
