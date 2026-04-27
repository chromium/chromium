// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/public/skill.h"

namespace skills {

Skill::Skill() = default;

Skill::Skill(const std::string& id,
             const std::string& name,
             const std::string& icon,
             const std::string& prompt,
             const std::string& description,
             const std::string& curated_by,
             const GURL& image_url,
             const sync_pb::SkillSource& source)
    : id(id),
      name(name),
      icon(icon),
      prompt(prompt),
      description(description),
      curated_by(curated_by),
      image_url(image_url),
      source(source) {}

Skill::Skill(const Skill&) = default;
Skill& Skill::operator=(const Skill&) = default;
Skill::Skill(Skill&&) = default;
Skill& Skill::operator=(Skill&&) = default;

Skill::~Skill() = default;

std::ostream& operator<<(std::ostream& os, const Skill& skill) {
  os << "{id: \"" << skill.id << "\", name: \"" << skill.name << "\", icon: \""
     << skill.icon << "\", prompt: \"" << skill.prompt << "\", description: \""
     << skill.description << "\", source_skill_id: \"" << skill.source_skill_id
     << "\", source: " << static_cast<int>(skill.source) << "}";
  return os;
}

}  // namespace skills
