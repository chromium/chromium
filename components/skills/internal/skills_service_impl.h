// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_INTERNAL_SKILLS_SERVICE_IMPL_H_
#define COMPONENTS_SKILLS_INTERNAL_SKILLS_SERVICE_IMPL_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skills_service.h"

namespace skills {

// This class is the main implementation of SkillsService. It is responsible for
// managing skills, maintaining contextually relevant skills, and invoke skills.
// It also notifies observers when skills are changed.
class SkillsServiceImpl : public SkillsService {
 public:
  SkillsServiceImpl();
  ~SkillsServiceImpl() override;

  // SkillsService implementation.
  const Skill* AddSkill(const std::string& name,
                        const std::string& icon,
                        const std::string& prompt) override;
  void LoadInitialSkills(
      std::vector<std::unique_ptr<Skill>> initial_skills) override;
  const std::vector<std::unique_ptr<Skill>>& GetSkills() const override;
  base::CallbackListSubscription RegisterSkillsChangedCallback(
      base::RepeatingClosure callback) override;

 private:
  void NotifySkillsChanged();

  // The list of skills managed by this service.
  std::vector<std::unique_ptr<Skill>> skills_;
  // The list of callbacks to be notified when the skills change.
  base::RepeatingClosureList skills_changed_callbacks_;

  // Weak pointer factory for posting tasks.
  base::WeakPtrFactory<SkillsServiceImpl> weak_ptr_factory_{this};
};

}  // namespace skills

#endif  // COMPONENTS_SKILLS_INTERNAL_SKILLS_SERVICE_IMPL_H_
