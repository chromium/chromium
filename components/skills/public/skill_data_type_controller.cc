// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/public/skill_data_type_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "components/prefs/pref_service.h"
#include "components/skills/public/skills_features.h"
#include "components/skills/public/skills_prefs.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service.h"

namespace skills {

SkillDataTypeController::SkillDataTypeController(
    syncer::SyncService* sync_service,
    PrefService* pref_service,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_transport_mode)
    : DataTypeController(syncer::SKILL,
                         std::move(delegate_for_full_sync_mode),
                         std::move(delegate_for_transport_mode)),
      sync_service_(sync_service),
      pref_service_(pref_service) {
  pref_registrar_.Init(pref_service_);
  // base::Unretained() is safe because `pref_registrar_` is owned by `this`.
  pref_registrar_.Add(
      prefs::kChromeSkillsEnabled,
      base::BindRepeating(&SkillDataTypeController::OnPrefChanged,
                          base::Unretained(this)));
}

SkillDataTypeController::~SkillDataTypeController() = default;

syncer::DataTypeController::PreconditionState
SkillDataTypeController::GetPreconditionState(
    const PreconditionContext& context) const {
  return IsSkillsEnabled(pref_service_)
             ? PreconditionState::kPreconditionsMet
             : PreconditionState::kMustStopAndClearData;
}

void SkillDataTypeController::OnPrefChanged() {
  sync_service_->DataTypePreconditionChanged(type());
}

}  // namespace skills
