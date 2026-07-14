// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_PUBLIC_SKILLS_PROVIDER_H_
#define COMPONENTS_SKILLS_PUBLIC_SKILLS_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "components/skills/public/skill.h"

namespace skills {

// Base interface for any source that vends skills to the SkillsService.
class SkillsProvider {
 public:
  using SkillsChangedCallback = base::RepeatingClosure;

  virtual ~SkillsProvider() = default;

  virtual base::CallbackListSubscription RegisterSkillsChangedCallback(
      SkillsChangedCallback callback) = 0;

  // Returns the current list of skills managed by this provider.
  virtual const std::vector<std::unique_ptr<Skill>>& GetSkills() const = 0;

  // Instructs the provider to refresh its skill data. The provider will
  // asynchronously update its internal state and invoke the
  // SkillsChangedCallback if the available skills have changed.
  virtual void RefreshSkills() = 0;
};

}  // namespace skills

#endif  // COMPONENTS_SKILLS_PUBLIC_SKILLS_PROVIDER_H_
