// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_MANAGER_H_

#include <string>

#include "chromeos/ash/components/boca/boca_session_manager.h"

namespace boca {
class UserIdentity;
}  // namespace boca

namespace ash::boca {

// Session manager implementation that is primarily used for configuring and
// managing OnTask components and services throughout a Boca session.
class BabelOrcaManager : public boca::BocaSessionManager::Observer {
 public:
  BabelOrcaManager();
  BabelOrcaManager(const BabelOrcaManager&) = delete;
  BabelOrcaManager& operator=(const BabelOrcaManager&) = delete;
  ~BabelOrcaManager() override;

  // BocaSessionManager::Observer:
  void OnSessionStarted(const std::string& session_id,
                        const ::boca::UserIdentity& producer) override;
  void OnSessionEnded(const std::string& session_id) override;

  bool IsCaptioningAvailable();
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_BABEL_ORCA_MANAGER_H_
