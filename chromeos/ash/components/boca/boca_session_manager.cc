// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/boca_session_manager.h"

#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"

namespace ash::boca {

BocaSessionManager::BocaSessionManager() = default;
BocaSessionManager::~BocaSessionManager() = default;

void BocaSessionManager::Observer::OnBundleUpdated(
    const ::boca::Bundle& bundle) {}

void BocaSessionManager::Observer::OnSessionCaptionConfigUpdated(
    const std::string& group_name,
    const ::boca::CaptionsConfig& config) {}

void BocaSessionManager::Observer::OnLocalCaptionConfigUpdated(
    const ::boca::CaptionsConfig& config) {}

void BocaSessionManager::Observer::OnSessionRosterUpdated(
    const std::string& group_name,
    const std::vector<::boca::UserIdentity>& consumers) {}

void BocaSessionManager::NotifyError(BocaError error) {}

void BocaSessionManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void BocaSessionManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}
}  // namespace ash::boca
