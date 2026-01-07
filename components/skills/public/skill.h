// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_PUBLIC_SKILL_H_
#define COMPONENTS_SKILLS_PUBLIC_SKILL_H_

#include <string>

namespace skills {

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

  Skill(const std::string& id,
        const std::string& name,
        const std::string& icon,
        const std::string& prompt);

  ~Skill();
};

}  // namespace skills

#endif  // COMPONENTS_SKILLS_PUBLIC_SKILL_H_
