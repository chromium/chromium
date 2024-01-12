// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/search/hierarchy.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/ash/settings/constants/constants_util.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_sections.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::settings {

namespace mojom {
using ::chromeos::settings::mojom::Section;
using ::chromeos::settings::mojom::Setting;
using ::chromeos::settings::mojom::Subpage;
}  // namespace mojom

namespace {

// Used to generate localized names.
constexpr double kDummyRelevanceScore = 0;

}  // namespace

class Hierarchy::PerSectionHierarchyGenerator
    : public OsSettingsSection::HierarchyGenerator {
 public:
  PerSectionHierarchyGenerator(mojom::Section section, Hierarchy* hierarchy)
      : section_(section), hierarchy_(hierarchy) {}

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
    CHECK_EQ(section_, metadata.primary.section)
        << "Setting registered in multiple primary sections: " << setting;
    CHECK(!metadata.primary.subpage)
        << "Setting registered in multiple primary locations: " << setting;
  }

  void RegisterNestedSetting(mojom::Setting setting,
                             mojom::Subpage subpage) override {
    Hierarchy::SettingMetadata& metadata = GetSettingMetadata(setting);
    CHECK_EQ(section_, metadata.primary.section)
        << "Setting registered in multiple primary sections: " << setting;
    CHECK(!metadata.primary.subpage)
        << "Setting registered in multiple primary locations: " << setting;
    metadata.primary.subpage = subpage;
  }

  void RegisterTopLevelAltSetting(mojom::Setting setting) override {
    Hierarchy::SettingMetadata& metadata = GetSettingMetadata(setting);
    CHECK(metadata.primary.section != section_ || metadata.primary.subpage)
        << "Setting's primary and alternate locations are identical: "
        << setting;
    for (const auto& alternate : metadata.alternates) {
      CHECK(alternate.section != section_ || alternate.subpage)
          << "Setting has multiple identical alternate locations: " << setting;
    }
    metadata.alternates.emplace_back(section_, /*subpage=*/std::nullopt);
  }

  void RegisterNestedAltSetting(mojom::Setting setting,
                                mojom::Subpage subpage) override {
    Hierarchy::SettingMetadata& metadata = GetSettingMetadata(setting);
    CHECK(metadata.primary.section != section_ ||
          metadata.primary.subpage != subpage)
        << "Setting's primary and alternate locations are identical: "
        << setting;
    for (const auto& alternate : metadata.alternates) {
      CHECK(alternate.section != section_ || alternate.subpage != subpage)
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

  mojom::Section section_;
  raw_ptr<Hierarchy> hierarchy_;
};

Hierarchy::SectionMetadata::SectionMetadata(mojom::Section section,
                                            const Hierarchy* hierarchy)
    : section_(section), hierarchy_(hierarchy) {}

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
      /*text=*/l10n_util::GetStringUTF16(name_message_id_),
      /*canonical_text=*/l10n_util::GetStringUTF16(name_message_id_),
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
    : primary(primary_section, /*subpage=*/std::nullopt) {}

Hierarchy::SettingMetadata::~SettingMetadata() = default;

Hierarchy::Hierarchy(const OsSettingsSections* sections) : sections_(sections) {
  for (const auto& section : AllSections()) {
    auto pair = section_map_.insert({section, SectionMetadata(section, this)});
    CHECK(pair.second);

    PerSectionHierarchyGenerator generator(section, this);
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
          ->text);
  return hierarchy_strings;
}

std::vector<std::u16string> Hierarchy::GenerateAncestorHierarchyStrings(
    mojom::Setting setting) const {
  const SettingMetadata& setting_metadata = GetSettingMetadata(setting);

  // Top-level setting; simply return section hierarchy.
  if (!setting_metadata.primary.subpage)
    return GenerateHierarchyStrings(setting_metadata.primary.section);

  // Nested setting; use subpage ancestors, then append subpage name itself.
  std::vector<std::u16string> hierarchy_strings =
      GenerateAncestorHierarchyStrings(*setting_metadata.primary.subpage);
  hierarchy_strings.push_back(
      GetSubpageMetadata(*setting_metadata.primary.subpage)
          .ToSearchResult(kDummyRelevanceScore)
          ->text);
  return hierarchy_strings;
}

std::vector<std::u16string> Hierarchy::GenerateHierarchyStrings(
    mojom::Section section) const {
  std::vector<std::u16string> hierarchy_strings;
  hierarchy_strings.push_back(
      l10n_util::GetStringUTF16(IDS_INTERNAL_APP_SETTINGS));
  hierarchy_strings.push_back(
      GetSectionMetadata(section).ToSearchResult(kDummyRelevanceScore)->text);
  return hierarchy_strings;
}

#ifdef DCHECK
namespace {

// Number of spaces for each indent level.
constexpr int kIndent = 2;

void PrintSettings(std::ostream& os,
                   int indent,
                   std::vector<mojom::Setting>& collection) {
  for (auto& it : collection) {
    os << std::string(indent, ' ') << "(s)" << it << std::endl;
  }
}

void PrintSubpages(
    std::ostream& os,
    int indent,
    std::vector<mojom::Subpage>& collection,
    std::map<mojom::Subpage, std::vector<mojom::Subpage>>& subpage_subpage,
    std::map<mojom::Subpage, std::vector<mojom::Setting>>& subpage_setting) {
  for (auto& it : collection) {
    os << std::string(indent, ' ') << "(p)" << it << std::endl;
    PrintSettings(os, indent + kIndent, subpage_setting[it]);
    PrintSubpages(os, indent + kIndent, subpage_subpage[it], subpage_subpage,
                  subpage_setting);
  }
}

}  // namespace

std::ostream& operator<<(std::ostream& os, const Hierarchy& h) {
  // This method logs all sections -> subpages -> settings

  // First restructure the `Hierarchy` data into hierarchies we can work with.
  std::map<mojom::Section, std::vector<mojom::Subpage>> section_subpage;
  std::map<mojom::Section, std::vector<mojom::Setting>> section_setting;
  std::map<mojom::Subpage, std::vector<mojom::Subpage>> subpage_subpage;
  std::map<mojom::Subpage, std::vector<mojom::Setting>> subpage_setting;
  std::vector<mojom::Subpage> none_exist_subpage;
  std::vector<mojom::Setting> none_exist_setting;

  for (auto& section_id : AllSections()) {
    section_subpage.insert({section_id, {}});
    section_setting.insert({section_id, {}});
  }

  for (auto& subpage_id : AllSubpages()) {
    subpage_subpage.insert({subpage_id, {}});
    subpage_setting.insert({subpage_id, {}});

    if (!base::Contains(h.subpage_map_, subpage_id)) {
      none_exist_subpage.push_back(subpage_id);
      continue;
    }

    auto& subpage = h.GetSubpageMetadata(subpage_id);
    if (subpage.parent_subpage) {
      // if this is a nested subpage, only record the immediate parent subpage.
      subpage_subpage[subpage.parent_subpage.value()].push_back(subpage_id);
    } else {
      section_subpage[subpage.section].push_back(subpage_id);
    }
  }

  for (auto& setting_id : AllSettings()) {
    if (!base::Contains(h.setting_map_, setting_id)) {
      none_exist_setting.push_back(setting_id);
      continue;
    }
    auto& setting = h.GetSettingMetadata(setting_id);

    if (setting.primary.subpage) {
      subpage_setting[setting.primary.subpage.value()].push_back(setting_id);
    } else {
      section_setting[setting.primary.section].push_back(setting_id);
    }

    for (auto& alt : setting.alternates) {
      if (alt.subpage) {
        subpage_setting[alt.subpage.value()].push_back(setting_id);
      } else {
        section_setting[alt.section].push_back(setting_id);
      }
    }
  }

  // Print out all the information.
  os << "Settings Hierarchy:" << std::endl;
  for (auto& section_id : AllSections()) {
    auto& subpages = section_subpage[section_id];
    auto& settings = section_setting[section_id];

    os << "[" << section_id << "]" << std::endl;
    PrintSubpages(os, kIndent, subpages, subpage_subpage, subpage_setting);
    PrintSettings(os, kIndent, settings);
  }

  os << "Unused Subpages: " << std::endl;
  PrintSubpages(os, kIndent, none_exist_subpage, subpage_subpage,
                subpage_setting);

  os << "Unused Settings: " << std::endl;
  PrintSettings(os, kIndent, none_exist_setting);

  return os;
}
#endif

}  // namespace ash::settings
