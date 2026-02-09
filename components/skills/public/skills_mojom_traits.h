// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_PUBLIC_SKILLS_MOJOM_TRAITS_H_
#define COMPONENTS_SKILLS_PUBLIC_SKILLS_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skill.mojom-shared.h"
#include "components/sync/protocol/skill_specifics.pb.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<skills::mojom::SkillSource, sync_pb::SkillSource> {
  static skills::mojom::SkillSource ToMojom(sync_pb::SkillSource input) {
    switch (input) {
      case sync_pb::SkillSource::SKILL_SOURCE_UNKNOWN:
        return skills::mojom::SkillSource::kUnknown;
      case sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY:
        return skills::mojom::SkillSource::kFirstParty;
      case sync_pb::SkillSource::SKILL_SOURCE_USER_CREATED:
        return skills::mojom::SkillSource::kUserCreated;
      case sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY:
        return skills::mojom::SkillSource::kDerivedFromFirstParty;
    }
    NOTREACHED();
  }

  static bool FromMojom(skills::mojom::SkillSource input,
                        sync_pb::SkillSource* out) {
    switch (input) {
      case skills::mojom::SkillSource::kUnknown:
        *out = sync_pb::SkillSource::SKILL_SOURCE_UNKNOWN;
        return true;
      case skills::mojom::SkillSource::kFirstParty:
        *out = sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY;
        return true;
      case skills::mojom::SkillSource::kUserCreated:
        *out = sync_pb::SkillSource::SKILL_SOURCE_USER_CREATED;
        return true;
      case skills::mojom::SkillSource::kDerivedFromFirstParty:
        *out = sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY;
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
  static const std::string& source_skill_id(const skills::Skill& skill) {
    return skill.source_skill_id;
  }
  static const std::string& prompt(const skills::Skill& skill) {
    return skill.prompt;
  }
  static sync_pb::SkillSource source(const skills::Skill& skill) {
    return skill.source;
  }
  static const std::string& description(const skills::Skill& skill) {
    return skill.description;
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
