// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_PUBLIC_SKILLS_MOJOM_TRAITS_H_
#define COMPONENTS_SKILLS_PUBLIC_SKILLS_MOJOM_TRAITS_H_

#include <optional>

#include "base/notreached.h"
#include "components/skills/public/skill.h"
#include "components/skills/public/skill.mojom-shared.h"
#include "components/sync/protocol/skill_specifics.pb.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "url/gurl.h"

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

  static sync_pb::SkillSource FromMojom(skills::mojom::SkillSource input) {
    switch (input) {
      case skills::mojom::SkillSource::kUnknown:
        return sync_pb::SkillSource::SKILL_SOURCE_UNKNOWN;
      case skills::mojom::SkillSource::kFirstParty:
        return sync_pb::SkillSource::SKILL_SOURCE_FIRST_PARTY;
      case skills::mojom::SkillSource::kUserCreated:
        return sync_pb::SkillSource::SKILL_SOURCE_USER_CREATED;
      case skills::mojom::SkillSource::kDerivedFromFirstParty:
        return sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY;
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
  static std::optional<std::string> curated_by(const skills::Skill& skill) {
    if (skill.curated_by.empty()) {
      return std::nullopt;
    }
    return skill.curated_by;
  }
  static std::optional<GURL> image_url(const skills::Skill& skill) {
    if (skill.image_url.is_empty()) {
      return std::nullopt;
    }
    return skill.image_url;
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
