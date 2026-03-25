// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/public/skills_service.h"

namespace skills {

bool SkillsService::Observer::Require1PSkillRefresh() {
  return false;
}

SkillsService::SkillsService() = default;

SkillsService::~SkillsService() = default;

}  // namespace skills
