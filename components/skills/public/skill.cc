// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/public/skill.h"

namespace skills {

Skill::Skill(const std::string& id,
             const std::string& name,
             const std::string& icon,
             const std::string& prompt)
    : id(id),
      name(name),
      icon(icon),
      prompt(prompt) {}

Skill::~Skill() = default;

}  // namespace skills
