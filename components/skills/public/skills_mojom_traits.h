// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_PUBLIC_SKILLS_MOJOM_TRAITS_H_
#define COMPONENTS_SKILLS_PUBLIC_SKILLS_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skill.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<skills::mojom::SkillSource, skills::SkillSource> {
  static skills::mojom::SkillSource ToMojom(skills::SkillSource input) {
    switch (input) {
      case skills::SkillSource::kUnknown:
        return skills::mojom::SkillSource::kUnknown;
      case skills::SkillSource::kFirstParty:
        return skills::mojom::SkillSource::kFirstParty;
      case skills::SkillSource::kUserCreated:
        return skills::mojom::SkillSource::kUserCreated;
    }
    NOTREACHED();
  }

  static bool FromMojom(skills::mojom::SkillSource input,
                        skills::SkillSource* out) {
    switch (input) {
      case skills::mojom::SkillSource::kUnknown:
        *out = skills::SkillSource::kUnknown;
        return true;
      case skills::mojom::SkillSource::kFirstParty:
        *out = skills::SkillSource::kFirstParty;
        return true;
      case skills::mojom::SkillSource::kUserCreated:
        *out = skills::SkillSource::kUserCreated;
        return true;
    }
    NOTREACHED();
  }
};

template <>
struct StructTraits<skills::mojom::SkillDataView, skills::Skill> {
  static const std::string& id(const skills::Skill& skill) { return skill.id; }
  static const std::string& name(const skills::Skill& skill) {
    return skill.name;
  }
  static const std::string& icon(const skills::Skill& skill) {
    return skill.icon;
  }
  static const std::string& prompt(const skills::Skill& skill) {
    return skill.prompt;
  }
  static skills::SkillSource source(const skills::Skill& skill) {
    return skill.source;
  }
  static base::Time creation_time(const skills::Skill& skill) {
    return skill.creation_time;
  }
  static base::Time last_update_time(const skills::Skill& skill) {
    return skill.last_update_time;
  }

  static bool Read(skills::mojom::SkillDataView data, skills::Skill* out);
};

}  // namespace mojo

#endif  // COMPONENTS_SKILLS_PUBLIC_SKILLS_MOJOM_TRAITS_H_
