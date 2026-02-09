// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_PUBLIC_SKILL_H_
#define COMPONENTS_SKILLS_PUBLIC_SKILL_H_

#include <string>

#include "base/time/time.h"
#include "components/sync/protocol/skill_specifics.pb.h"

namespace skills {

// LINT.IfChange(Skill)
// Represents a single skill.
struct Skill {
  // A unique identifier for the skill. It's GUID now but can be other IDs in
  // the future.
  std::string id;

  // The ID of the source skill this skill is derived from.
  std::string source_skill_id;

  // The user-facing name of the skill.
  std::string name;

  // The icon for the skill.
  std::string icon;

  // The underlying LLM prompt for the skill.
  std::string prompt;

  // The description of the skill.
  std::string description;

  // The source of the skill which can be 1P or user created.
  sync_pb::SkillSource source = sync_pb::SkillSource::SKILL_SOURCE_USER_CREATED;

  // The time when the skill was created.
  base::Time creation_time = base::Time::Now();

  // The time when the skill was last updated.
  base::Time last_update_time = creation_time;

  Skill();
  Skill(const std::string& id,
        const std::string& name,
        const std::string& icon,
        const std::string& prompt,
        const std::string& description = "",
        const sync_pb::SkillSource& source =
            sync_pb::SkillSource::SKILL_SOURCE_USER_CREATED);
  Skill(const Skill&);
  Skill& operator=(const Skill&);
  Skill(Skill&&);
  Skill& operator=(Skill&&);
  ~Skill();
};
// LINT.ThenChange(//components/skills/public/skill.mojom:Skill,
// //chrome/browser/glic/host/glic.mojom:Skill)

}  // namespace skills

#endif  // COMPONENTS_SKILLS_PUBLIC_SKILL_H_
