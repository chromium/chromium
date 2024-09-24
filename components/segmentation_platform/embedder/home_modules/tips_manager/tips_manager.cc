// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/embedder/home_modules/tips_manager/tips_manager.h"

#include "components/prefs/pref_service.h"
#include "components/segmentation_platform/embedder/home_modules/tips_manager/constants.h"

namespace segmentation_platform {

TipsManager::TipsManager(PrefService* pref_service,
                         PrefService* local_pref_service)
    : pref_service_(pref_service), local_pref_service_(local_pref_service) {
  CHECK(pref_service_);
  CHECK(local_pref_service_);
}

void TipsManager::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  pref_service_ = nullptr;
  local_pref_service_ = nullptr;
}

void TipsManager::NotifySignal(const std::string& signal) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/369211657): Implement `NotifySignal()` to handle tip-related
  // events.
}

}  // namespace segmentation_platform
