// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIVERSAL_OPTOUT_UNIVERSAL_OPTOUT_SERVICE_H_
#define COMPONENTS_UNIVERSAL_OPTOUT_UNIVERSAL_OPTOUT_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace universal_optout {

// Service responsible for tracking location history and determining eligibility
// of users for Universal Opt Out.
class UniversalOptOutService : public KeyedService {
 public:
  explicit UniversalOptOutService(PrefService* pref_service);

  UniversalOptOutService(const UniversalOptOutService&) = delete;
  UniversalOptOutService& operator=(const UniversalOptOutService&) = delete;

  ~UniversalOptOutService() override;

  // KeyedService:
  void Shutdown() override;

  // Returns whether the profile is eligible for Universal Opt Out.
  bool IsEligible() const;

 private:
  raw_ptr<PrefService> pref_service_;
};

}  // namespace universal_optout

#endif  // COMPONENTS_UNIVERSAL_OPTOUT_UNIVERSAL_OPTOUT_SERVICE_H_
