// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/babelorca/babel_orca_manager.h"

#include <string>

#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"

namespace ash::boca {

BabelOrcaManager::BabelOrcaManager() = default;
BabelOrcaManager::~BabelOrcaManager() = default;

void BabelOrcaManager::OnSessionStarted(const std::string& session_id,
                                        const ::boca::UserIdentity& producer) {}

void BabelOrcaManager::OnSessionEnded(const std::string& session_id) {}

bool BabelOrcaManager::IsCaptioningAvailable() {
  // TODO(b/361086008): Implement IsCaptioningAvailable();
  return true;
}

}  // namespace ash::boca
