// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_ISOLATED_MODE_ISOLATED_MODE_SETTINGS_SERVICE_H_
#define COMPONENTS_ENTERPRISE_ISOLATED_MODE_ISOLATED_MODE_SETTINGS_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/version_info/channel.h"

class PrefService;

namespace enterprise_isolated_mode {

class IsolatedModeSettingsService : public KeyedService {
 public:
  IsolatedModeSettingsService(PrefService* prefs,
                              version_info::Channel channel);
  IsolatedModeSettingsService(const IsolatedModeSettingsService&) = delete;
  IsolatedModeSettingsService& operator=(const IsolatedModeSettingsService&) =
      delete;
  ~IsolatedModeSettingsService() override = default;

  // Returns the cached evaluation of whether Isolated Mode should replace
  // Incognito.
  bool ReplacesIncognito() const { return replaces_incognito_; }

 private:
  const bool replaces_incognito_;
};

}  // namespace enterprise_isolated_mode

#endif  // COMPONENTS_ENTERPRISE_ISOLATED_MODE_ISOLATED_MODE_SETTINGS_SERVICE_H_
