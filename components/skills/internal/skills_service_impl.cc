// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/skills_service_impl.h"

#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "components/skills/internal/skills_sync_bridge.h"
#include "components/skills/public/skill.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_controller_delegate.h"

namespace skills {

SkillsServiceImpl::SkillsServiceImpl(
    version_info::Channel channel,
    syncer::OnceDataTypeStoreFactory create_store_callback) {
  // TODO(crbug.com/471795213): consider using a common flag to control the
  // whole service.
  if (base::FeatureList::IsEnabled(syncer::kSyncSkill)) {
    // TODO(crbug.com/471795213): consider removing a dependency on the channel.
    sync_bridge_ = std::make_unique<SkillsSyncBridge>(
        std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
            syncer::SKILL,
            base::BindRepeating(&syncer::ReportUnrecoverableError, channel)),
        std::move(create_store_callback));
  }
}

SkillsServiceImpl::~SkillsServiceImpl() = default;

void SkillsServiceImpl::NotifySkillsChanged() {
  skills_changed_callbacks_.Notify();
}

const Skill* SkillsServiceImpl::AddSkill(const std::string& name,
                                         const std::string& icon,
                                         const std::string& prompt) {
  auto skill = std::make_unique<Skill>(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), name, icon, prompt);
  Skill* const new_skill_ptr = skill.get();
  skills_.push_back(std::move(skill));
  // This is added to avoid Reentrancy.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SkillsServiceImpl::NotifySkillsChanged,
                                weak_ptr_factory_.GetWeakPtr()));
  return new_skill_ptr;
}

void SkillsServiceImpl::LoadInitialSkills(
    std::vector<std::unique_ptr<Skill>> initial_skills) {
  skills_ = std::move(initial_skills);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SkillsServiceImpl::NotifySkillsChanged,
                                weak_ptr_factory_.GetWeakPtr()));
}

const std::vector<std::unique_ptr<Skill>>& SkillsServiceImpl::GetSkills()
    const {
  return skills_;
}

base::CallbackListSubscription SkillsServiceImpl::RegisterSkillsChangedCallback(
    base::RepeatingClosure callback) {
  return skills_changed_callbacks_.Add(std::move(callback));
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
SkillsServiceImpl::GetControllerDelegate() {
  if (sync_bridge_) {
    return sync_bridge_->change_processor()->GetControllerDelegate();
  }
  return nullptr;
}

}  // namespace skills
