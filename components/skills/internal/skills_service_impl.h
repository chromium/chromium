// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_INTERNAL_SKILLS_SERVICE_IMPL_H_
#define COMPONENTS_SKILLS_INTERNAL_SKILLS_SERVICE_IMPL_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/uuid.h"
#include "base/version_info/channel.h"
#include "components/skills/internal/skills_downloader.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_service.h"
#include "components/sync/model/data_type_store.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace optimization_guide {
class OptimizationGuideDecider;
}  // namespace optimization_guide

namespace skills {

class SkillsSyncBridge;

// This class is the main implementation of SkillsService. It is responsible for
// managing skills, maintaining contextually relevant skills, and invoke skills.
// It also notifies observers when skills are changed.
class SkillsServiceImpl : public SkillsService {
 public:
  SkillsServiceImpl(
      optimization_guide::OptimizationGuideDecider* optimization_guide,
      version_info::Channel channel,
      syncer::OnceDataTypeStoreFactory create_store_callback,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~SkillsServiceImpl() override;

  // KeyedService implementation.
  void Shutdown() override;

  // SkillsService implementation.
  ServiceStatus GetServiceStatus() const override;
  void LoadInitialSkills(
      std::vector<std::unique_ptr<Skill>> initial_skills) override;
  // TODO(crbug.com/475863107) Add strong typing to help caller avoid swapping
  // order of arguments.
  const Skill* AddSkill(const std::string& source_skill_id,
                        const std::string& name,
                        const std::string& icon,
                        const std::string& prompt) override;

  const Skill* AddOrUpdateSkillFromSync(std::string_view skill_id,
                                        std::string_view source_skill_id,
                                        std::string_view name,
                                        std::string_view icon,
                                        std::string_view prompt,
                                        std::string_view description,
                                        base::Time creation_time,
                                        base::Time last_update_time,
                                        sync_pb::SkillSource source) override;

  // TODO(crbug.com/475863107) Add strong typing to help caller avoid swapping
  // order of arguments.
  const Skill* UpdateSkill(std::string_view skill_id,
                           std::string_view name,
                           std::string_view icon,
                           std::string_view prompt) override;

  void DeleteSkill(std::string_view skill_id,
                   UpdateSource update_source) override;
  const Skill* GetSkillById(std::string_view skill_id) const override;
  void FetchDiscoverySkills() override;
  void Handle1pSkillsMap(std::unique_ptr<SkillsMap> skills_map) override;
  const SkillsMap& Get1PSkills() const override;
  const std::vector<std::unique_ptr<Skill>>& GetSkills() const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override;
  void SyncStatusChanged() override;
  void SetServiceStatusForTesting(ServiceStatus status) override;

 private:
  void NotifySkillChanged(std::string_view skill_id,
                          UpdateSource update_source,
                          bool is_position_changed);

  // Adds a skill to the service and returns the created skill.
  const Skill* AddSkillImpl(std::unique_ptr<Skill> skill,
                            UpdateSource update_source);

  // Returns a mutable skill with the given ID or nullptr if not found.
  Skill* GetMutableSkillById(std::string_view skill_id);

  // Returns the position of the skill with the given ID or nullopt if not
  // found.
  std::optional<size_t> GetSkillPosition(std::string_view skill_id) const;

  // Updates an existing `skill` with the given data. `update_time` is used only
  // if the skill is actually updated with new data or if updated from sync.
  void UpdateSkillImpl(Skill* skill,
                       std::string_view name,
                       std::string_view icon,
                       std::string_view prompt,
                       std::string_view description,
                       base::Time update_time,
                       UpdateSource update_source);

  // Whether the service is initialized, i.e. LoadInitialSkills() has been
  // called.
  bool is_initialized_ = false;

  // Sorts the skills by name in alphabetical order.
  void SortSkills();

  // The list of skills managed by this service.
  std::vector<std::unique_ptr<Skill>> skills_;

  // The map of loaded 1p discovery skills.
  SkillsMap first_party_skills_map_;

  // The list of observers to be notified on changes.
  base::ObserverList<Observer,
                     /*check_empty=*/true,
                     base::ObserverListReentrancyPolicy::kDisallowReentrancy>
      observers_;

  // Sync bridge for skills.
  std::unique_ptr<SkillsSyncBridge> sync_bridge_;

  // Downloader for 1P skills.
  std::unique_ptr<SkillsDownloader> skills_downloader_;

  // Service status for testing purposes which overrides the actual service
  // status.
  std::optional<ServiceStatus> service_status_for_testing_;

  // Weak pointer factory for posting tasks.
  base::WeakPtrFactory<SkillsServiceImpl> weak_ptr_factory_{this};
};

}  // namespace skills

#endif  // COMPONENTS_SKILLS_INTERNAL_SKILLS_SERVICE_IMPL_H_
