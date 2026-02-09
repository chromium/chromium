// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/public/skills_mojom_traits.h"

#include "base/uuid.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<skills::mojom::SkillDataView, skills::Skill>::Read(
    skills::mojom::SkillDataView data,
    skills::Skill* out) {
  std::optional<std::string> source_skill_id;
  if (!data.ReadId(&out->id) || !data.ReadName(&out->name) ||
      !data.ReadIcon(&out->icon) || !data.ReadSourceSkillId(&source_skill_id) ||
      !data.ReadPrompt(&out->prompt) || !data.ReadSource(&out->source) ||
      !data.ReadDescription(&out->description) ||
      !data.ReadCreationTime(&out->creation_time) ||
      !data.ReadLastUpdateTime(&out->last_update_time)) {
    return false;
  }

  const bool is_derived_from_first_party =
      out->source ==
      sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY;
  // Check if the source_skill_id is populated that the source must be
  // kDerivedFromFirstParty and that the id is a valid UUID.
  if (source_skill_id && !source_skill_id->empty()) {
    const bool is_valid_uuid =
        base::Uuid::ParseCaseInsensitive(*source_skill_id).is_valid();
    if (!is_valid_uuid || !is_derived_from_first_party) {
      return false;
    }
  } else if (is_derived_from_first_party) {
    // If the source_skill_id is not populated or empty and the source is set as
    // kDerivedFromFirstParty then it is also invalid.
    return false;
  }

  out->source_skill_id = source_skill_id.value_or("");
  return true;
}

}  // namespace mojo
