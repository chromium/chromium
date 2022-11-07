// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_IDENTIFIERS_PROFILE_ID_SERVICE_H_
#define COMPONENTS_ENTERPRISE_BROWSER_IDENTIFIERS_PROFILE_ID_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace enterprise {

class ProfileIdDelegate;

// This service creates a profile identifier for the current profile. It is
// important to note that profile identifiers are created for non OTR profiles
// i.e profiles that are not in guest or incognito modes.
class ProfileIdService : public KeyedService {
 public:
  ProfileIdService(std::unique_ptr<ProfileIdDelegate> delegate,
                   PrefService* profile_prefs);

  ~ProfileIdService() override;

  // Creates and returns the profile identifier for the current profile.
  absl::optional<std::string> GetProfileId();

 private:
  std::unique_ptr<ProfileIdDelegate> delegate_;
  raw_ptr<PrefService> profile_prefs_;
};

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_BROWSER_IDENTIFIERS_PROFILE_ID_SERVICE_H_
