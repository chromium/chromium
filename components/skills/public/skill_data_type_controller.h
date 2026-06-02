// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_PUBLIC_SKILL_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SKILLS_PUBLIC_SKILL_DATA_TYPE_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/service/data_type_controller.h"

class PrefService;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace skills {

// DataTypeController for syncer::SKILL.
// It disables sync if skills are disabled via pref.
class SkillDataTypeController : public syncer::DataTypeController {
 public:
  SkillDataTypeController(syncer::SyncService* sync_service,
                          PrefService* pref_service,
                          std::unique_ptr<syncer::DataTypeControllerDelegate>
                              delegate_for_full_sync_mode,
                          std::unique_ptr<syncer::DataTypeControllerDelegate>
                              delegate_for_transport_mode);

  SkillDataTypeController(const SkillDataTypeController&) = delete;
  SkillDataTypeController& operator=(const SkillDataTypeController&) = delete;

  ~SkillDataTypeController() override;

  // syncer::DataTypeController implementation.
  PreconditionState GetPreconditionState(
      const PreconditionContext& context) const override;

 private:
  void OnPrefChanged();

  const raw_ptr<syncer::SyncService> sync_service_;
  const raw_ptr<PrefService> pref_service_;

  PrefChangeRegistrar pref_registrar_;
};

}  // namespace skills

#endif  // COMPONENTS_SKILLS_PUBLIC_SKILL_DATA_TYPE_CONTROLLER_H_
