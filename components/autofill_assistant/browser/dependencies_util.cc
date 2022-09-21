// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/dependencies_util.h"

#include <string>

#include "components/variations/service/variations_service.h"

using ::variations::VariationsService;

namespace autofill_assistant::dependencies_util {

std::string GetLatestCountryCode(VariationsService* variations_service) {
  if (!variations_service || variations_service->GetLatestCountry().empty()) {
    // Use fallback "ZZ" if no country is available.
    return "ZZ";
  }
  return base::ToUpperASCII(variations_service->GetLatestCountry());
}

std::string GetStoredPermanentCountryCode(
    variations::VariationsService* variations_service) {
  if (!variations_service ||
      variations_service->GetStoredPermanentCountry().empty()) {
    // Use fallback "ZZ" if no country is available.
    return "ZZ";
  }
  return base::ToUpperASCII(variations_service->GetStoredPermanentCountry());
}

}  // namespace autofill_assistant::dependencies_util
