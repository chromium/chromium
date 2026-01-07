// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_PUBLIC_SKILLS_SERVICE_H_
#define COMPONENTS_SKILLS_PUBLIC_SKILLS_SERVICE_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "components/keyed_service/core/keyed_service.h"

namespace skills {

struct Skill;

// Core service for managing skills.
class SkillsService : public KeyedService {
 public:
  SkillsService();
  ~SkillsService() override;

  // Adds a new skill.
  // Generates a unique ID for the skill.
  // Returns a const pointer to the newly added skill.
  virtual const Skill* AddSkill(const std::string& name,
                                const std::string& icon,
                                const std::string& prompt) = 0;

  // Loads a skill list into memory.
  virtual void LoadInitialSkills(
      std::vector<std::unique_ptr<Skill>> initial_skills) = 0;

  // Returns a const reference to the currently loaded skills.
  virtual const std::vector<std::unique_ptr<Skill>>& GetSkills() const = 0;

  // Registers a callback to be notified when the skills change.
  virtual base::CallbackListSubscription RegisterSkillsChangedCallback(
      base::RepeatingClosure callback) = 0;
};

}  // namespace skills

#endif  // COMPONENTS_SKILLS_PUBLIC_SKILLS_SERVICE_H_
