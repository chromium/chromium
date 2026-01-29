// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_PUBLIC_SKILL_H_
#define COMPONENTS_SKILLS_PUBLIC_SKILL_H_

#include <string>

#include "base/time/time.h"

namespace skills {

// LINT.IfChange(SkillSource)
enum class SkillSource {
  kUnknown = 0,
  // Skill created by Google.
  kFirstParty = 1,
  // Skill created by an end-user.
  kUserCreated = 2,
};
// LINT.ThenChange(//depot/chromium/components/skills/public/skill.mojom:SkillSource,
// //depot/chromium/chrome/browser/glic/host/glic.mojom:SkillSource)

// LINT.IfChange(Skill)
// Represents a single skill.
struct Skill {
  // A unique identifier for the skill. It's GUID now but can be other IDs in
  // the future.
  const std::string id;

  // The user-facing name of the skill.
  std::string name;

  // The icon for the skill.
  std::string icon;

  // The underlying LLM prompt for the skill.
  std::string prompt;

  // The source of the skill which can be 1P or user created.
  SkillSource source = SkillSource::kUserCreated;

  // The time when the skill was created.
  base::Time creation_time = base::Time::Now();

  // The time when the skill was last updated.
  base::Time last_update_time = creation_time;

  Skill(const std::string& id,
        const std::string& name,
        const std::string& icon,
        const std::string& prompt);
  Skill(const Skill& other) = delete;
  Skill& operator=(const Skill& other) = delete;
  ~Skill();
};
// LINT.ThenChange(//depot/chromium/components/skills/public/skill.mojom:Skill,
// //depot/chromium/chrome/browser/glic/host/glic.mojom:Skill)

}  // namespace skills

#endif  // COMPONENTS_SKILLS_PUBLIC_SKILL_H_
