// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_CRD_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_CRD_MANAGER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
namespace ash::boca {

class SpotlightCrdManager {
 public:
  SpotlightCrdManager(const SpotlightCrdManager&) = delete;
  SpotlightCrdManager& operator=(const SpotlightCrdManager&) = delete;
  virtual ~SpotlightCrdManager() = default;

  virtual void OnSessionStarted(const std::string& teacher_email) = 0;
  virtual void OnSessionEnded() = 0;

  // Starts the CRD session and returns the connection code for the request.
  virtual void InitiateSpotlightSession(
      base::OnceCallback<void(std::optional<std::string>)> callback) = 0;

 protected:
  SpotlightCrdManager() = default;
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_CRD_MANAGER_H_
