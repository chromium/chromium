// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/hierarchy.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/constants_util.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_section.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_sections.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace settings {
namespace {

// Used to generate localized names.
constexpr double kDummyRelevanceScore = 0;

}  // namespace

class Hierarchy::PerSectionHierarchyGenerator
    : public OsSettingsSection::HierarchyGenerator {
 public:
  PerSectionHierarchyGenerator(mojom::Section section,
                               bool* only_contains_link_to_subpage,
                               Hierarchy* hierarchy)
      : section_(section),
        only_contains_link_to_subpage_(only_contains_link_to_subpage),
        hierarchy_(hierarchy) {}

  void RegisterTopLevelSubpage(
      int name_message_id,
      mojom::Subpage subpage,
      mojom::SearchResultIcon icon,
      mojom::SearchResultDefaultRank default_rank,
      const std::string& url_path_with_parameters) override {
    Hierarchy::SubpageMetadata& metadata = GetSubpageMetadata(
        name_message_id, subpage, icon, default_rank, url_path_with_parameters);
    CHECK_EQ(section_, metadata.section)
        << "Subpage registered in multiple sections: " << subpage;

    ++num_top_level_subpages_so_far_;

    // If there are multiple top-level subpages, the section contains more than
    // just a link to a subpage.
    if (num_top_level_subpages_so_far_ > 1u)
      *only_contains_link_to_subpage_ = false;
  }

  void RegisterNestedSubpage(
      int name_message_id,
      mojom::Subpage subpage,
      mojom::Subpage parent_subpage,
      mojom::SearchResultIcon icon,
      mojom::SearchResultDefaultRank default_rank,
      const std::string& url_path_with_parameters) override {
    Hierarchy::SubpageMetadata& metadata = GetSubpageMetadata(
        name_message_id, subpage, icon, default_rank, url_path_with_parameters);
    CHECK_EQ(section_, metadata.section)
        << "Subpage registered in multiple sections: " << subpage;
    CHECK(!metadata.parent_subpage)
        << "Subpage has multiple registered parent subpages: " << subpage;
    metadata.parent_subpage = parent_subpage;
  }

  void RegisterTopLevelSetting(mojom::Setting setting) override {
    Hierarchy::SettingMetadata& metadata = GetSettingMetadata(setting);
    CHECK_EQ(section_, metadata.primary.first)
        << "Setting registered in multiple primary sections: " << setting;
    CHECK(!metadata.primary.second)
        << "Setting registered in multiple primary locations: " << setting;

    // If a top-level setting exists, the section contains more than just a link
    // to a subpage.
    *only_contains_link_to_subpage_ = false;
  }

  void RegisterNestedSetting(mojom::Setting setting,
                             mojom::Subpage subpage) override {
    Hierarchy::SettingMetadata& metadata = GetSettingMetadata(setting);
    CHECK_EQ(section_, metadata.primary.first)
        << "Setting registered in multiple primary sections: " << setting;
    CHECK(!metadata.primary.second)
        << "Setting registered in multiple primary locations: " << setting;
    metadata.primary.second = subpage;
  }

  void RegisterTopLevelAltSetting(mojom::Setting setting) override {
    Hierarchy::SettingMetadata& metadata = GetSettingMetadata(setting);
    CHECK(metadata.primary.first != section_ || metadata.primary.second)
        << "Setting's primary and alternate locations are identical: "
        << setting;
    for (const auto& alternate : metadata.alternates) {
      CHECK(alternate.first != section_ || alternate.second)
          << "Setting has multiple identical alternate locations: " << setting;
    }
    metadata.alternates.emplace_back(section_, /*subpage=*/base::nullopt);

    // If a top-level setting exists, the section contains more than just a link
    // to a subpage.
    *only_contains_link_to_subpage_ = false;
  }

  void RegisterNestedAltSetting(mojom::Setting setting,
                                mojom::Subpage subpage) override {
    Hierarchy::SettingMetadata& metadata = GetSettingMetadata(setting);
    CHECK(metadata.primary.first != section_ ||
          metadata.primary.second != subpage)
        << "Setting's primary and alternate locations are identical: "
        << setting;
    for (const auto& alternate : metadata.alternates) {
      CHECK(alternate.first != section_ || alternate.second != subpage)
          << "Setting has multiple identical alternate locations: " << setting;
    }
    metadata.alternates.emplace_back(section_, subpage);
  }

 private:
  Hierarchy::SubpageMetadata& GetSubpageMetadata(
      int name_message_id,
      mojom::Subpage subpage,
      mojom::SearchResultIcon icon,
      mojom::SearchResultDefaultRank default_rank,
      const std::string& url_path_with_parameters) {
    auto& subpage_map = hierarchy_->subpage_map_;

    auto it = subpage_map.find(subpage);

    // Metadata already exists; return it.
    if (it != subpage_map.end())
      return it->second;

    // Metadata does not exist yet; insert then return it.
    auto pair = subpage_map.emplace(
        std::piecewise_construct, std::forward_as_tuple(subpage),
        std::forward_as_tuple(name_message_id, section_, subpage, icon,
                              default_rank, url_path_with_parameters,
                              hierarchy_));
    CHECK(pair.second);
    return pair.first->second;
  }

  Hierarchy::SettingMetadata& GetSettingMetadata(mojom::Setting setting) {
    auto& settings_map = hierarchy_->setting_map_;

    auto it = settings_map.find(setting);

    // Metadata already exists; return it.
    if (it != settings_map.end())
      return it->second;

    // Metadata does not exist yet; insert then return it.
    auto pair = settings_map.emplace(setting, section_);
    CHECK(pair.second);
    return pair.first->second;
  }

  size_t num_top_level_subpages_so_far_ = 0u;
  mojom::Section section_;
  bool* only_contains_link_to_subpage_;
  Hierarchy* hierarchy_;
};

// Note: |only_contains_link_to_subpage| starts out as true and is set to false
// if other content is added.
Hierarchy::SectionMetadata::SectionMetadata(mojom::Section section,
                                            const Hierarchy* hierarchy)
    : only_contains_link_to_subpage(true),
      section_(section),
      hierarchy_(hierarchy) {}

Hierarchy::SectionMetadata::~SectionMetadata() = default;

mojom::SearchResultPtr Hierarchy::SectionMetadata::ToSearchResult(
    double relevance_score) const {
  return hierarchy_->sections_->GetSection(section_)
      ->GenerateSectionSearchResult(relevance_score);
}

Hierarchy::SubpageMetadata::SubpageMetadata(
    int name_message_id,
    mojom::Section section,
    mojom::Subpage subpage,
    mojom::SearchResultIcon icon,
    mojom::SearchResultDefaultRank default_rank,
    const std::string& url_path_with_parameters,
    const Hierarchy* hierarchy)
    : section(section),
      subpage_(subpage),
      name_message_id_(name_message_id),
      icon_(icon),
      default_rank_(default_rank),
      unmodified_url_path_with_parameters_(url_path_with_parameters),
      hierarchy_(hierarchy) {}

Hierarchy::SubpageMetadata::~SubpageMetadata() = default;

mojom::SearchResultPtr Hierarchy::SubpageMetadata::ToSearchResult(
    double relevance_score) const {
  return mojom::SearchResult::New(
      /*result_text=*/l10n_util::GetStringUTF16(name_message_id_),
      /*canonical_result_text=*/l10n_util::GetStringUTF16(name_message_id_),
      hierarchy_->ModifySearchResultUrl(
          section, mojom::SearchResultType::kSubpage, {.subpage = subpage_},
          unmodified_url_path_with_parameters_),
      icon_, relevance_score,
      hierarchy_->GenerateAncestorHierarchyStrings(subpage_), default_rank_,
      /*was_generated_from_text_match=*/false,
      mojom::SearchResultType::kSubpage,
      mojom::SearchResultIdentifier::NewSubpage(subpage_));
}

Hierarchy::SettingMetadata::SettingMetadata(mojom::Section primary_section)
    : primary(primary_section, /*subpage=*/base::nullopt) {}

Hierarchy::SettingMetadata::~SettingMetadata() = default;

Hierarchy::Hierarchy(const OsSettingsSections* sections) : sections_(sections) {
  for (const auto& section : constants::AllSections()) {
    auto pair = section_map_.emplace(std::piecewise_construct,
                                     std::forward_as_tuple(section),
                                     std::forward_as_tuple(section, this));
    CHECK(pair.second);

    PerSectionHierarchyGenerator generator(
        section, &pair.first->second.only_contains_link_to_subpage, this);
    sections->GetSection(section)->RegisterHierarchy(&generator);
  }
}

Hierarchy::~Hierarchy() = default;

const Hierarchy::SectionMetadata& Hierarchy::GetSectionMetadata(
    mojom::Section section) const {
  const auto it = section_map_.find(section);
  CHECK(it != section_map_.end())
      << "Section missing from settings hierarchy: " << section;
  return it->second;
}

const Hierarchy::SubpageMetadata& Hierarchy::GetSubpageMetadata(
    mojom::Subpage subpage) const {
  const auto it = subpage_map_.find(subpage);
  CHECK(it != subpage_map_.end())
      << "Subpage missing from settings hierarchy: " << subpage;
  return it->second;
}

const Hierarchy::SettingMetadata& Hierarchy::GetSettingMetadata(
    mojom::Setting setting) const {
  const auto it = setting_map_.find(setting);
  CHECK(it != setting_map_.end())
      << "Setting missing from settings hierarchy: " << setting;
  return it->second;
}

std::string Hierarchy::ModifySearchResultUrl(
    mojom::Section section,
    mojom::SearchResultType type,
    OsSettingsIdentifier id,
    const std::string& url_to_modify) const {
  return sections_->GetSection(section)->ModifySearchResultUrl(type, id,
                                                               url_to_modify);
}

std::vector<std::u16string> Hierarchy::GenerateAncestorHierarchyStrings(
    mojom::Subpage subpage) const {
  const SubpageMetadata& subpage_metadata = GetSubpageMetadata(subpage);

  // Top-level subpage; simply return section hierarchy.
  if (!subpage_metadata.parent_subpage)
    return GenerateHierarchyStrings(subpage_metadata.section);

  // Nested subpage; use recursive call, then append parent subpage name itself.
  std::vector<std::u16string> hierarchy_strings =
      GenerateAncestorHierarchyStrings(*subpage_metadata.parent_subpage);
  hierarchy_strings.push_back(
      GetSubpageMetadata(*subpage_metadata.parent_subpage)
          .ToSearchResult(kDummyRelevanceScore)
          ->result_text);
  return hierarchy_strings;
}

std::vector<std::u16string> Hierarchy::GenerateAncestorHierarchyStrings(
    mojom::Setting setting) const {
  const SettingMetadata& setting_metadata = GetSettingMetadata(setting);

  // Top-level setting; simply return section hierarchy.
  if (!setting_metadata.primary.second)
    return GenerateHierarchyStrings(setting_metadata.primary.first);

  // Nested setting; use subpage ancestors, then append subpage name itself.
  std::vector<std::u16string> hierarchy_strings =
      GenerateAncestorHierarchyStrings(*setting_metadata.primary.second);
  hierarchy_strings.push_back(
      GetSubpageMetadata(*setting_metadata.primary.second)
          .ToSearchResult(kDummyRelevanceScore)
          ->result_text);
  return hierarchy_strings;
}

std::vector<std::u16string> Hierarchy::GenerateHierarchyStrings(
    mojom::Section section) const {
  std::vector<std::u16string> hierarchy_strings;
  hierarchy_strings.push_back(
      l10n_util::GetStringUTF16(IDS_INTERNAL_APP_SETTINGS));
  hierarchy_strings.push_back(GetSectionMetadata(section)
                                  .ToSearchResult(kDummyRelevanceScore)
                                  ->result_text);
  return hierarchy_strings;
}

}  // namespace settings
}  // namespace chromeos
