// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/skills_service_impl.h"

#include "base/check_is_test.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/skills/features.h"
#include "components/skills/internal/skills_downloader.h"
#include "components/skills/internal/skills_sync_bridge.h"
#include "components/skills/public/skill.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/protocol/skill_specifics.pb.h"

namespace skills {

namespace {
// Minimum time between discovery skills refreshes.
constexpr base::TimeDelta kMinimumTimeBetweenDiscoverySkillsRefresh =
    base::Hours(2);
}  // namespace

SkillsServiceImpl::SkillsServiceImpl(
    optimization_guide::OptimizationGuideDecider* optimization_guide,
    signin::IdentityManager* identity_manager,
    version_info::Channel channel,
    syncer::OnceDataTypeStoreFactory create_store_callback,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory) {
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
  skills_downloader_ = std::make_unique<SkillsDownloader>(url_loader_factory_);

  discovery_skills_refresh_timer_.Start(
      FROM_HERE, kMinimumTimeBetweenDiscoverySkillsRefresh, this,
      &SkillsServiceImpl::RefreshDiscoverySkills);
}

SkillsServiceImpl::~SkillsServiceImpl() = default;

void SkillsServiceImpl::Shutdown() {
  for (Observer& observer : observers_) {
    observer.OnSkillsServiceShuttingDown();
  }
}

void SkillsServiceImpl::NotifySkillChanged(std::string_view skill_id,
                                           UpdateSource update_source,
                                           bool is_position_changed) {
  for (Observer& observer : observers_) {
    observer.OnSkillUpdated(skill_id, update_source, is_position_changed);
  }
}

void SkillsServiceImpl::NotifyTemporarySkillDisplayChanged(
    std::string_view skill_id,
    DisplayState display_state) {
  for (Observer& observer : observers_) {
    observer.OnTemporarySkillDisplay(skill_id, display_state);
  }
}

const Skill* SkillsServiceImpl::AddSkill(const std::string& source_skill_id,
                                         const std::string& name,
                                         const std::string& icon,
                                         const std::string& prompt) {
  if (GetServiceStatus() != ServiceStatus::kReady) {
    return nullptr;
  }

  auto skill = std::make_unique<Skill>(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), name, icon, prompt);
  skill->source_skill_id = source_skill_id;
  // If the skill has a source skill id, it is a derived skill.
  if (!source_skill_id.empty()) {
    skill->source = sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY;
  }
  return AddSkillImpl(std::move(skill), UpdateSource::kLocal);
}

const Skill* SkillsServiceImpl::AddOrUpdateSkillFromSync(
    std::string_view skill_id,
    std::string_view source_skill_id,
    std::string_view name,
    std::string_view icon,
    std::string_view prompt,
    std::string_view description,
    base::Time creation_time,
    base::Time last_update_time,
    sync_pb::SkillSource source) {
  CHECK_EQ(GetServiceStatus(), ServiceStatus::kReady);

  if (Skill* skill = GetMutableSkillById(skill_id)) {
    // Skill already exists, update its fields.
    UpdateSkillImpl(skill, name, icon, prompt, description, last_update_time,
                    UpdateSource::kSync);
    return skill;
  }

  auto skill = std::make_unique<Skill>(std::string(skill_id), std::string(name),
                                       std::string(icon), std::string(prompt),
                                       std::string(description));
  skill->source_skill_id = source_skill_id;
  // Use the creation and last update time from sync to keep them in sync with
  // other clients.
  skill->creation_time = creation_time;
  skill->last_update_time = last_update_time;
  skill->source = source;
  return AddSkillImpl(std::move(skill), UpdateSource::kSync);
}

const Skill* SkillsServiceImpl::UpdateSkill(std::string_view skill_id,
                                            std::string_view name,
                                            std::string_view icon,
                                            std::string_view prompt) {
  if (GetServiceStatus() != ServiceStatus::kReady) {
    return nullptr;
  }

  Skill* skill = GetMutableSkillById(skill_id);
  if (!skill) {
    // Skill does not exist, nothing to update.
    return nullptr;
  }

  UpdateSkillImpl(skill, name, icon, prompt, /*description=*/"",
                  /*update_time=*/base::Time::Now(), UpdateSource::kLocal);
  return skill;
}

void SkillsServiceImpl::DeleteSkill(std::string_view skill_id,
                                    UpdateSource update_source) {
  const std::string id_copy(skill_id);
  const size_t num_erased =
      std::erase_if(skills_, [&id_copy](const std::unique_ptr<Skill>& skill) {
        return skill->id == id_copy;
      });

  if (num_erased > 0) {
    NotifySkillChanged(id_copy, update_source, /*is_position_changed=*/false);
  }
}

const Skill* SkillsServiceImpl::GetSkillById(std::string_view skill_id) const {
  // A skill can be either a 1st party skill, or a user generated skill.
  // First, Attempt to retrieve the skill from the definitive list of 1P
  // skills.
  auto it = first_party_skill_objects_map_.find(skill_id);
  if (it != first_party_skill_objects_map_.end()) {
    return &it->second;
  }

  std::optional<size_t> skill_position = GetSkillPosition(skill_id);
  if (!skill_position.has_value()) {
    return nullptr;
  }

  return skills_[*skill_position].get();
}

const std::vector<std::unique_ptr<Skill>>& SkillsServiceImpl::GetSkills()
    const {
  return skills_;
}

const SkillProtoList& SkillsServiceImpl::Get1PSkills() const {
  return first_party_data_.skills_list;
}

const std::vector<skills::proto::TopicInfo>&
SkillsServiceImpl::Get1PTopicsInfo() const {
  return first_party_data_.topics_info_list;
}

void SkillsServiceImpl::LoadInitialSkills(
    std::vector<std::unique_ptr<Skill>> initial_skills) {
  CHECK(!is_initialized_);
  skills_ = std::move(initial_skills);
  SortSkills();

  is_initialized_ = true;

  for (Observer& observer : observers_) {
    observer.OnStatusChanged();
  }
}

SkillsService::ServiceStatus SkillsServiceImpl::GetServiceStatus() const {
  if (service_status_for_testing_.has_value()) {
    CHECK_IS_TEST();
    return *service_status_for_testing_;
  }
  if (!is_initialized_) {
    return ServiceStatus::kNotInitialized;
  }
  if (!sync_bridge_->change_processor()->IsTrackingMetadata()) {
    return ServiceStatus::kInitializedWaitingForSyncReady;
  }
  return ServiceStatus::kReady;
}

void SkillsServiceImpl::SortSkills() {
  std::sort(
      skills_.begin(), skills_.end(),
      [](const std::unique_ptr<Skill>& a, const std::unique_ptr<Skill>& b) {
        return a->last_update_time > b->last_update_time;
      });
}

void SkillsServiceImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  observer->OnStatusChanged();
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

void SkillsServiceImpl::SyncStatusChanged() {
  for (Observer& observer : observers_) {
    observer.OnStatusChanged();
  }
}

void SkillsServiceImpl::SetServiceStatusForTesting(ServiceStatus status) {
  service_status_for_testing_ = status;
  for (Observer& observer : observers_) {
    observer.OnStatusChanged();
  }
}

const Skill* SkillsServiceImpl::AddSkillImpl(std::unique_ptr<Skill> skill,
                                             UpdateSource update_source) {
  // Added skill must not exist in the service.
  CHECK(!GetSkillById(skill->id));

  const Skill* skill_ptr = skill.get();
  skills_.push_back(std::move(skill));
  SortSkills();
  NotifySkillChanged(skill_ptr->id, update_source,
                     /*is_position_changed=*/true);
  return skill_ptr;
}

void SkillsServiceImpl::FetchDiscoverySkills() {
  if (!base::FeatureList::IsEnabled(features::kSkillsEnabled)) {
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kSkillsServiceApi) &&
      identity_manager_ &&
      identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    if (!skills_fetcher_) {
      skills_fetcher_ = std::make_unique<SkillsFetcher>(url_loader_factory_,
                                                        identity_manager_);
    }
    skills_fetcher_->FetchDiscoverySkills(
        base::BindOnce(&SkillsServiceImpl::OnDiscoverySkillsFetchedFromService,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  skills_downloader_->FetchDiscoverySkills(base::BindOnce(
      &SkillsServiceImpl::Handle1pSkills, weak_ptr_factory_.GetWeakPtr()));
}

#if !BUILDFLAG(IS_ANDROID)
void SkillsServiceImpl::OnDiscoverySkillsFetchedFromService(
    std::unique_ptr<FirstPartySkillData> first_party_skill_data) {
  if (first_party_skill_data) {
    Handle1pSkills(std::move(first_party_skill_data));
    return;
  }
  // Fallback to downloader if API failed.
  skills_downloader_->FetchDiscoverySkills(base::BindOnce(
      &SkillsServiceImpl::Handle1pSkills, weak_ptr_factory_.GetWeakPtr()));
}
#endif  // !BUILDFLAG(IS_ANDROID)

void SkillsServiceImpl::Handle1pSkills(
    std::unique_ptr<FirstPartySkillData> first_party_skill_data) {
  last_discovery_skills_fetch_time_ = base::Time::Now();
  FirstPartySkillData* notification_ptr = nullptr;
  // If first_party_skill_data is null, this means we don't have an updated
  // value so we shouldn't modify the stored 1p data.
  if (first_party_skill_data) {
    first_party_data_ = std::move(*first_party_skill_data);
    notification_ptr = &first_party_data_;

    first_party_skill_objects_map_.clear();
    first_party_skill_objects_map_.reserve(
        first_party_data_.skills_list.size());
    for (const auto& proto_skill : first_party_data_.skills_list) {
      Skill skill(proto_skill.id(), proto_skill.name(), proto_skill.icon(),
                  proto_skill.prompt(), proto_skill.description(),
                  proto_skill.curated_by(), GURL(proto_skill.image_url()),
                  sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY);
      first_party_skill_objects_map_.insert(
          {proto_skill.id(), std::move(skill)});
    }
  }

  for (Observer& observer : observers_) {
    observer.OnDiscoverySkillsUpdated(notification_ptr);
  }
}

Skill* SkillsServiceImpl::GetMutableSkillById(std::string_view skill_id) {
  return const_cast<Skill*>(GetSkillById(skill_id));
}

std::optional<size_t> SkillsServiceImpl::GetSkillPosition(
    std::string_view skill_id) const {
  for (size_t i = 0; i < skills_.size(); ++i) {
    if (skills_[i]->id == skill_id) {
      return i;
    }
  }
  return std::nullopt;
}

void SkillsServiceImpl::UpdateSkillImpl(Skill* skill,
                                        std::string_view name,
                                        std::string_view icon,
                                        std::string_view prompt,
                                        std::string_view description,
                                        base::Time update_time,
                                        UpdateSource update_source) {
  CHECK(skill);

  // Update the existing skill.
  std::optional<size_t> old_position = GetSkillPosition(skill->id);

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
  if (skill->description != description) {
    skill->description = description;
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
    SortSkills();

    const bool is_position_changed =
        old_position != GetSkillPosition(skill->id);
    NotifySkillChanged(skill->id, update_source, is_position_changed);
  }
}

void SkillsServiceImpl::NotifyPanelWillOpen() {
  RefreshDiscoverySkills();
}

void SkillsServiceImpl::RefreshDiscoverySkills() {
  if (base::Time::Now() - last_discovery_skills_fetch_time_ <
      kMinimumTimeBetweenDiscoverySkillsRefresh) {
    // If the discovery skills have been fetched recently, do not refresh them
    // again.
    return;
  }

  // Check if any observers require a refresh of discovery skills.
  // Note: call to FetchDiscoverySkills needs to be made outside of traversal
  // of the observers list. Otherwise, if FetchDiscoverySkills returns too
  // quickly, Handle1pSkills will be called, triggering another traversal of
  // observers list. The observers list is configured so that it can only be
  // traverse once at a time. This race condition would cause a crash.
  bool requires_refresh = false;
  for (Observer& observer : observers_) {
    // If there are no glic panels currently open and needs to display
    // 1P skills, do not fetch discovery skills. This avoids unnecessary
    // fetches.
    if (observer.Require1PSkillRefresh()) {
      requires_refresh = true;
      break;
    }
  }

  if (requires_refresh) {
    FetchDiscoverySkills();
  }
}

}  // namespace skills
