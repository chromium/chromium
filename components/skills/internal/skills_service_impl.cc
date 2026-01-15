// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/skills_service_impl.h"

#include "base/notimplemented.h"
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

namespace {

auto IdMatches(std::string_view target_id) {
  return [target_id](const std::unique_ptr<Skill>& skill) {
    return skill->id == target_id;
  };
}

}  // namespace

SkillsServiceImpl::SkillsServiceImpl(
    version_info::Channel channel,
    syncer::OnceDataTypeStoreFactory create_store_callback) {
  // TODO(crbug.com/471795213): consider using a common flag to control the
  // whole service.
  if (base::FeatureList::IsEnabled(syncer::kSyncSkill)) {
    sync_bridge_ = std::make_unique<SkillsSyncBridge>(
        std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
            syncer::SKILL,
            base::BindRepeating(&syncer::ReportUnrecoverableError, channel)),
        std::move(create_store_callback), *this);
  }
}

SkillsServiceImpl::~SkillsServiceImpl() = default;

void SkillsServiceImpl::NotifySkillChanged(const std::string& skill_id) {
  for (Observer& observer : observers_) {
    observer.OnSkillUpdated(skill_id);
  }
}

const Skill* SkillsServiceImpl::AddSkill(const std::string& name,
                                         const std::string& icon,
                                         const std::string& prompt) {
  // TODO(crbug.com/475855831): Add a check to ensure service is initialized.
  auto skill = std::make_unique<Skill>(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), name, icon, prompt);
  const Skill* skill_ptr = skill.get();
  skills_.push_back(std::move(skill));
  NotifySkillChanged(skill_ptr->id);
  return skill_ptr;
}

const Skill* SkillsServiceImpl::UpdateSkill(std::string_view skill_id,
                                            std::string_view name,
                                            std::string_view icon,
                                            std::string_view prompt) {
  // TODO(crbug.com/475855831): Add a check to ensure service is initialized.
  auto it = std::find_if(skills_.begin(), skills_.end(), IdMatches(skill_id));
  if (it == skills_.end()) {
    // Skill not found.
    return nullptr;
  }

  Skill* skill = it->get();

  // Update the existing skill.
  bool is_changed = false;
  if (skill->name != name) {
    skill->name = name;
    is_changed = true;
  }
  if (skill->icon != icon) {
    skill->icon = icon;
    is_changed = true;
  }
  if (skill->prompt != prompt) {
    skill->prompt = prompt;
    is_changed = true;
  }
  if (is_changed) {
    NotifySkillChanged(skill->id);
  }

  return skill;
}

void SkillsServiceImpl::DeleteSkill(std::string_view skill_id) {
  // TODO(crbug.com/475855831): Add a check to ensure service is initialized.

  auto it = std::find_if(skills_.begin(), skills_.end(), IdMatches(skill_id));
  if (it == skills_.end()) {
    // Skill not found.
    return;
  }

  std::string id_copy(skill_id);
  skills_.erase(it);

  NotifySkillChanged(id_copy);
}

const Skill* SkillsServiceImpl::GetSkillById(
    const std::string_view& skill_id) const {
  for (const std::unique_ptr<Skill>& skill : skills_) {
    if (skill->id == skill_id) {
      return skill.get();
    }
  }
  return nullptr;
}

const std::vector<std::unique_ptr<Skill>>& SkillsServiceImpl::GetSkills()
    const {
  return skills_;
}

void SkillsServiceImpl::LoadInitialSkills(
    std::vector<std::unique_ptr<Skill>> initial_skills) {
  CHECK(!is_initialized_);
  skills_ = std::move(initial_skills);
  SortSkills();
  is_initialized_ = true;

  for (Observer& observer : observers_) {
    observer.OnInitialized();
  }
}

void SkillsServiceImpl::SortSkills() {
  std::sort(skills_.begin(), skills_.end(),
            [](const std::unique_ptr<Skill>& a,
               const std::unique_ptr<Skill>& b) { return a->name < b->name; });
}

void SkillsServiceImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SkillsServiceImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
SkillsServiceImpl::GetControllerDelegate() {
  if (sync_bridge_) {
    return sync_bridge_->change_processor()->GetControllerDelegate();
  }
  return nullptr;
}

}  // namespace skills
