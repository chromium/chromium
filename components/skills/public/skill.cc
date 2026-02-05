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
             const sync_pb::SkillSource& source)
    : id(id),
      name(name),
      icon(icon),
      prompt(prompt),
      description(description),
      source(source) {}

Skill::Skill(const Skill&) = default;
Skill& Skill::operator=(const Skill&) = default;
Skill::Skill(Skill&&) = default;
Skill& Skill::operator=(Skill&&) = default;

Skill::~Skill() = default;

}  // namespace skills
