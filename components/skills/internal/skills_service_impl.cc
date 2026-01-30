// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/skills_service_impl.h"

#include "base/notimplemented.h"
#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/skills/features.h"
#include "components/skills/internal/skills_downloader.h"
#include "components/skills/internal/skills_sync_bridge.h"
#include "components/skills/public/skill.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_controller_delegate.h"

namespace skills {

SkillsServiceImpl::SkillsServiceImpl(
    optimization_guide::OptimizationGuideDecider* optimization_guide,
    version_info::Channel channel,
    syncer::OnceDataTypeStoreFactory create_store_callback,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  sync_bridge_ = std::make_unique<SkillsSyncBridge>(
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::SKILL,
          base::BindRepeating(&syncer::ReportUnrecoverableError, channel)),
      std::move(create_store_callback), *this);
  if (base::FeatureList::IsEnabled(features::kSkillsEnabled)) {
    // If the Skills feature is enabled, register the optimization type to
    // signal to Optimization Guide that it should fetch and cache the URL-keyed
    // Skills on each page load.
    if (optimization_guide) {
      optimization_guide->RegisterOptimizationTypes(
          {optimization_guide::proto::SKILLS});
    }
  }
  skills_downloader_ =
      std::make_unique<SkillsDownloader>(std::move(url_loader_factory));
}

SkillsServiceImpl::~SkillsServiceImpl() = default;

void SkillsServiceImpl::NotifySkillChanged(std::string_view skill_id,
                                           UpdateSource update_source) {
  for (Observer& observer : observers_) {
    observer.OnSkillUpdated(skill_id, update_source);
  }
}

const Skill* SkillsServiceImpl::AddSkill(const std::string& name,
                                         const std::string& icon,
                                         const std::string& prompt) {
  CHECK(is_initialized_);

  auto skill = std::make_unique<Skill>(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), name, icon, prompt);
  return AddSkillImpl(std::move(skill), UpdateSource::kLocal);
}

const Skill* SkillsServiceImpl::AddOrUpdateSkillFromSync(
    std::string_view skill_id,
    std::string_view name,
    std::string_view icon,
    std::string_view prompt,
    base::Time creation_time,
    base::Time last_update_time) {
  CHECK(is_initialized_);

  if (Skill* skill = GetMutableSkillById(skill_id)) {
    // Skill already exists, update its fields.
    UpdateSkillImpl(skill, name, icon, prompt, last_update_time,
                    UpdateSource::kSync);
    return skill;
  }

  auto skill = std::make_unique<Skill>(std::string(skill_id), std::string(name),
                                       std::string(icon), std::string(prompt));
  // Use the creation and last update time from sync to keep them in sync with
  // other clients.
  skill->creation_time = creation_time;
  skill->last_update_time = last_update_time;
  return AddSkillImpl(std::move(skill), UpdateSource::kSync);
}

const Skill* SkillsServiceImpl::UpdateSkill(std::string_view skill_id,
                                            std::string_view name,
                                            std::string_view icon,
                                            std::string_view prompt) {
  CHECK(is_initialized_);

  Skill* skill = GetMutableSkillById(skill_id);
  if (!skill) {
    // Skill does not exist, nothing to update.
    return nullptr;
  }

  UpdateSkillImpl(skill, name, icon, prompt,
                  /*update_time=*/base::Time::Now(), UpdateSource::kLocal);
  return skill;
}

void SkillsServiceImpl::DeleteSkill(std::string_view skill_id,
                                    UpdateSource update_source) {
  CHECK(is_initialized_);

  // TODO(crbug.com/475855831): Add a check to ensure service is initialized.
  const std::string id_copy(skill_id);
  const size_t num_erased =
      std::erase_if(skills_, [&id_copy](const std::unique_ptr<Skill>& skill) {
        return skill->id == id_copy;
      });

  if (num_erased > 0) {
    NotifySkillChanged(id_copy, update_source);
  }
}

const Skill* SkillsServiceImpl::GetSkillById(std::string_view skill_id) const {
  CHECK(is_initialized_);
  for (const std::unique_ptr<Skill>& skill : skills_) {
    if (skill->id == skill_id) {
      return skill.get();
    }
  }
  return nullptr;
}

const std::vector<std::unique_ptr<Skill>>& SkillsServiceImpl::GetSkills()
    const {
  CHECK(is_initialized_);
  return skills_;
}

void SkillsServiceImpl::LoadInitialSkills(
    std::vector<std::unique_ptr<Skill>> initial_skills) {
  CHECK(!is_initialized_);
  skills_ = std::move(initial_skills);
  SortSkills();

  // TODO(crbug.com/471795213): consider using tracking metadata to determine if
  // the initialization is complete.
  is_initialized_ = true;

  for (Observer& observer : observers_) {
    observer.OnInitialized();
  }
}

bool SkillsServiceImpl::IsInitialized() const {
  return is_initialized_;
}

void SkillsServiceImpl::SortSkills() {
  std::sort(skills_.begin(), skills_.end(),
            [](const std::unique_ptr<Skill>& a,
               const std::unique_ptr<Skill>& b) { return a->name < b->name; });
}

void SkillsServiceImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  if (is_initialized_) {
    observer->OnInitialized();
  }
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

const Skill* SkillsServiceImpl::AddSkillImpl(std::unique_ptr<Skill> skill,
                                             UpdateSource update_source) {
  // Added skill must not exist in the service.
  CHECK(!GetSkillById(skill->id));

  const Skill* skill_ptr = skill.get();
  skills_.push_back(std::move(skill));
  NotifySkillChanged(skill_ptr->id, update_source);
  return skill_ptr;
}

void SkillsServiceImpl::FetchDiscoverySkills() {
  if (!base::FeatureList::IsEnabled(features::kSkillsEnabled)) {
    return;
  }
  skills_downloader_->FetchDiscoverySkills(base::BindOnce(
      &SkillsServiceImpl::Handle1pSkillsMap, weak_ptr_factory_.GetWeakPtr()));
}

void SkillsServiceImpl::Handle1pSkillsMap(
    std::unique_ptr<SkillsMap> skills_map) {
  for (Observer& observer : observers_) {
    observer.OnDiscoverySkillsUpdated(std::move(skills_map));
  }
}

Skill* SkillsServiceImpl::GetMutableSkillById(std::string_view skill_id) {
  return const_cast<Skill*>(GetSkillById(skill_id));
}

void SkillsServiceImpl::UpdateSkillImpl(Skill* skill,
                                        std::string_view name,
                                        std::string_view icon,
                                        std::string_view prompt,
                                        base::Time update_time,
                                        UpdateSource update_source) {
  CHECK(skill);

  // First party skills are not owned by the user. They cannot be updated.
  // Instead, the user should copy the skill content, so that the new, copied
  // skill is user created, then update the copied skill.
  CHECK(skill->source == SkillSource::kUserCreated)
      << "Skill does not belong to the user. Cannot update skill.";

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

  if (update_source == UpdateSource::kSync &&
      skill->last_update_time < update_time) {
    // Mark the skill as changed to update its last update time and notify
    // observers. This is relevant for sync updates only to keep the
    // `last_update_time` in sync with other clients.
    is_changed = true;
  }

  if (is_changed) {
    skill->last_update_time = update_time;
    NotifySkillChanged(skill->id, update_source);
  }
}

}  // namespace skills
