// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/dummy_ukm_data_manager.h"

#include "base/notreached.h"

namespace segmentation_platform {

DummyUkmDataManager::DummyUkmDataManager() = default;
DummyUkmDataManager::~DummyUkmDataManager() = default;

void DummyUkmDataManager::Initialize(const base::FilePath& database_path) {}

bool DummyUkmDataManager::IsUkmEngineEnabled() {
  return false;
}

void DummyUkmDataManager::NotifyCanObserveUkm(
    ukm::UkmRecorderImpl* ukm_recorder,
    PrefService* pref_service) {}

void DummyUkmDataManager::StartObservingUkm(const UkmConfig& config) {}

void DummyUkmDataManager::PauseOrResumeObservation(bool pause) {}

void DummyUkmDataManager::StopObservingUkm() {}

UrlSignalHandler* DummyUkmDataManager::GetOrCreateUrlHandler() {
  NOTREACHED();
  return nullptr;
}

UkmDatabase* DummyUkmDataManager::GetUkmDatabase() {
  NOTREACHED();
  return nullptr;
}

void DummyUkmDataManager::AddRef() {}

void DummyUkmDataManager::RemoveRef() {}

void DummyUkmDataManager::OnUkmAllowedStateChanged(bool allowed) {}

}  // namespace segmentation_platform
