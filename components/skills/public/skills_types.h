// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_PUBLIC_SKILLS_TYPES_H_
#define COMPONENTS_SKILLS_PUBLIC_SKILLS_TYPES_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "components/skills/proto/skill.pb.h"
#include "components/skills/public/skill.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace skills {

// Map of Skill IDs to its proto representation.
using SkillIdToProtoMap =
    absl::flat_hash_map<std::string, skills::proto::Skill>;

// Map of Skill categories to a vector of Skill structs.
using SkillCategoryToSkillMap = base::flat_map<std::string, std::vector<Skill>>;

// Map of Skill IDs to Skill structs.
using SkillIdToSkillMap = absl::flat_hash_map<std::string, Skill>;

}  // namespace skills

#endif  // COMPONENTS_SKILLS_PUBLIC_SKILLS_TYPES_H_
