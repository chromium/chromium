// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/skills_service_impl.h"

#include "base/task/single_thread_task_runner.h"
#include "base/uuid.h"
#include "components/skills/public/skill.h"

namespace skills {

SkillsServiceImpl::SkillsServiceImpl() = default;
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

}  // namespace skills
