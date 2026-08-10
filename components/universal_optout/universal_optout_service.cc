// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/universal_optout/universal_optout_service.h"

#include "components/prefs/pref_service.h"
#include "components/universal_optout/prefs.h"

namespace universal_optout {

UniversalOptOutService::UniversalOptOutService(PrefService* pref_service)
    : pref_service_(pref_service) {}

UniversalOptOutService::~UniversalOptOutService() = default;

void UniversalOptOutService::Shutdown() {}

bool UniversalOptOutService::IsEligible() const {
  if (!pref_service_) {
    return false;
  }
  return pref_service_->GetBoolean(prefs::kUniversalOptOutEligible);
}

}  // namespace universal_optout
