// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_BROWSER_IDENTIFIERS_PROFILE_ID_SERVICE_H_
#define COMPONENTS_ENTERPRISE_BROWSER_IDENTIFIERS_PROFILE_ID_SERVICE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace enterprise {

class ProfileIdDelegate;

// This service creates a profile identifier for the current profile. It is
// important to note that profile identifiers are created for non OTR profiles
// i.e profiles that are not in guest or incognito modes.
class ProfileIdService : public KeyedService {
 public:
  // Possible errors for the profile id generation. This must be kept in
  // sync with the EnterpriseProfileIdError UMA enum.
  enum class Error {
    kGetDeviceIdFailure,
    kGetProfileGUIDFailure,
    kProfileIdURLEncodeFailure,
    kMaxValue = kProfileIdURLEncodeFailure,
  };

  ProfileIdService(std::unique_ptr<ProfileIdDelegate> delegate,
                   PrefService* profile_prefs);

  // Used in tests to set a fake profile id.
  explicit ProfileIdService(const std::string profile_id);

  ~ProfileIdService() override;

  // Creates and returns the profile identifier for the current profile.
  std::optional<std::string> GetProfileId();

  std::optional<std::string> GetProfileIdWithGuidAndDeviceId(
      const std::string profile_guid,
      const std::string device_id);

 private:
  std::unique_ptr<ProfileIdDelegate> delegate_;
  raw_ptr<PrefService> profile_prefs_;
};

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_BROWSER_IDENTIFIERS_PROFILE_ID_SERVICE_H_
