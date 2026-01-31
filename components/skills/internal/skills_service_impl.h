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
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_service.h"
#include "components/sync/model/data_type_store.h"

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
      syncer::OnceDataTypeStoreFactory create_store_callback);
  ~SkillsServiceImpl() override;

  // SkillsService implementation.
  bool IsInitialized() const override;
  void LoadInitialSkills(
      std::vector<std::unique_ptr<Skill>> initial_skills) override;
  // TODO(crbug.com/475863107) Add strong typing to help caller avoid swapping
  // order of arguments.
  const Skill* AddSkill(const std::string& name,
                        const std::string& icon,
                        const std::string& prompt) override;

  const Skill* AddSkillFromSync(std::string_view skill_id,
                                std::string_view name,
                                std::string_view icon,
                                std::string_view prompt) override;

  // TODO(crbug.com/475863107) Add strong typing to help caller avoid swapping
  // order of arguments.
  const Skill* UpdateSkill(std::string_view skill_id,
                           std::string_view name,
                           std::string_view icon,
                           std::string_view prompt,
                           UpdateSource update_source) override;

  void DeleteSkill(std::string_view skill_id,
                   UpdateSource update_source) override;
  const Skill* GetSkillById(std::string_view skill_id) const override;
  const std::vector<std::unique_ptr<Skill>>& GetSkills() const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override;

 private:
  void NotifySkillChanged(std::string_view skill_id,
                          UpdateSource update_source);

  // Adds a skill to the service and returns the created skill.
  const Skill* AddSkillImpl(std::unique_ptr<Skill> skill,
                            UpdateSource update_source);

  // Returns a mutable skill with the given ID or nullptr if not found.
  Skill* GetMutableSkillById(std::string_view skill_id);

  // Whether the service is initialized.
  bool is_initialized_ = false;

  // Sorts the skills by name in alphabetical order.
  void SortSkills();

  // The list of skills managed by this service.
  std::vector<std::unique_ptr<Skill>> skills_;

  // The list of observers to be notified on changes.
  base::ObserverList<Observer, /*check_empty=*/true, /*allow_reentrancy=*/false>
      observers_;

  // Sync bridge for skills.
  std::unique_ptr<SkillsSyncBridge> sync_bridge_;

  // Weak pointer factory for posting tasks.
  base::WeakPtrFactory<SkillsServiceImpl> weak_ptr_factory_{this};
};

}  // namespace skills

#endif  // COMPONENTS_SKILLS_INTERNAL_SKILLS_SERVICE_IMPL_H_
