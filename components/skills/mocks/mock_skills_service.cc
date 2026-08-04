// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/mocks/mock_skills_service.h"

namespace skills {

MockSkillsService::MockSkillsService() {
  ON_CALL(*this, GetSkills).WillByDefault(testing::ReturnRef(empty_skills_));
  ON_CALL(*this, GetProvidedSkills)
      .WillByDefault(testing::ReturnRef(empty_provided_skill_objects_map_));
}
MockSkillsService::~MockSkillsService() = default;

}  // namespace skills
