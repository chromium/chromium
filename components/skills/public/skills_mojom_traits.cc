// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/public/skills_mojom_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<skills::mojom::SkillDataView, skills::Skill>::Read(
    skills::mojom::SkillDataView data,
    skills::Skill* out) {
  return data.ReadId(&out->id) && data.ReadName(&out->name) &&
         data.ReadIcon(&out->icon) && data.ReadPrompt(&out->prompt) &&
         data.ReadSource(&out->source) &&
         data.ReadCreationTime(&out->creation_time) &&
         data.ReadLastUpdateTime(&out->last_update_time);
}

}  // namespace mojo
