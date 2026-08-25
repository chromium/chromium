// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_generator.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/fixed_flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/functional/function_ref.h"
#include "base/i18n/char_iterator.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-forward.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom-shared.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips_metrics.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/remote_suggestions_service_simple.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/tab_id_generator.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/fusebox_action.mojom.h"
#include "components/omnibox/browser/fusebox_action_mojo_utils.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "third_party/omnibox_proto/groups.pb.h"
#include "third_party/omnibox_proto/input_type.pb.h"
#include "third_party/omnibox_proto/input_type_config.pb.h"
#include "third_party/omnibox_proto/page_vertical.pb.h"
#include "third_party/omnibox_proto/suggest_inventory.pb.h"
#include "third_party/omnibox_proto/suggest_template_info.pb.h"
#include "third_party/omnibox_proto/types.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/url_util.h"

namespace {
using ::action_chips::RecordActionChipsRequestStatus;
using ::action_chips::RemoteSuggestionsServiceSimple;
using ::action_chips::RemoteSuggestionsServiceSimpleImpl;
using ::action_chips::mojom::ActionChip;
using ::action_chips::mojom::ActionChipPtr;
using ::action_chips::mojom::IconType;
using ::action_chips::mojom::SuggestTemplateInfo;
using ::action_chips::mojom::SuggestTemplateInfoPtr;
using ::action_chips::mojom::TabInfo;
using ::action_chips::mojom::TabInfoPtr;
using ::tabs::TabInterface;

const size_t kMaxActionChips = 3;

size_t GetMaxNumChips() {
  return static_cast<size_t>(
      base::FeatureList::IsEnabled(ntp_features::kNtpScaledActionChipsSmall)
          ? ntp_features::kNtpMaxSmallChips.Get()
          : kMaxActionChips);
}

bool IsScaledChipsEnabled() {
  return base::FeatureList::IsEnabled(ntp_features::kNtpScaledActionChips) ||
         base::FeatureList::IsEnabled(ntp_features::kNtpScaledActionChipsSmall);
}

template <typename T>
void AssignMojoField(const T& source, T& dest) {
  dest = source;
}

template <typename T>
void AssignMojoField(const T& source, std::optional<T>& dest) {
  dest = source;
}

template <typename ProtoEnum, typename MojoEnum>
  requires std::is_enum_v<ProtoEnum> && std::is_enum_v<MojoEnum>
void AssignMojoField(const ProtoEnum& source, MojoEnum& dest) {
  dest = static_cast<MojoEnum>(source);
}

template <typename ProtoEnum, typename MojoEnum>
  requires std::is_enum_v<ProtoEnum> && std::is_enum_v<MojoEnum>
void AssignMojoField(const ProtoEnum& source, std::optional<MojoEnum>& dest) {
  dest = static_cast<MojoEnum>(source);
}

template <typename Proto, typename MojoPtr>
void SyncProtoToMojo(const Proto& a, MojoPtr& b);

template <typename ProtoChild, typename MojoChildPtr>
void AssignMojoField(const ProtoChild& source, MojoChildPtr& dest) {
  if (!dest) {
    dest = MojoChildPtr::element_type::New();
  }
  SyncProtoToMojo(source, dest);
}

// Specializations for known mappings.

// Helper to make static_assert dependent on template parameters.
// This is necessary because static_assert(false, ...) would be evaluated at
// definition time, causing compilation failure even if the function is never
// instantiated. By making it dependent on T, evaluation is deferred to
// instantiation time.
template <typename... T>
struct AlwaysFalse : std::false_type {};

template <typename ProtoA, typename MojoBPtr>
void SyncProtoToMojo(const ProtoA& a, MojoBPtr& b) {
  static_assert(AlwaysFalse<ProtoA, MojoBPtr>::value,
                "SyncProtoToMojo is not implemented to convert from the given "
                "Proto type to the given Mojo type. Please add a "
                "specialization.");
}

template <>
void SyncProtoToMojo<omnibox::FormattedString,
                     action_chips::mojom::FormattedStringPtr>(
    const omnibox::FormattedString& a,
    action_chips::mojom::FormattedStringPtr& b) {
  if (a.has_text()) {
    AssignMojoField(a.text(), b->text);
  }
  if (a.has_a11y_text()) {
    AssignMojoField(a.a11y_text(), b->a11y_text);
  }
}

template <>
void SyncProtoToMojo<omnibox::SuggestTemplateInfo,
                     action_chips::mojom::SuggestTemplateInfoPtr>(
    const omnibox::SuggestTemplateInfo& a,
    action_chips::mojom::SuggestTemplateInfoPtr& b) {
  if (a.has_type_icon()) {
    AssignMojoField(a.type_icon(), b->type_icon);
  }

  if (a.has_primary_text()) {
    AssignMojoField(a.primary_text(), b->primary_text);
  }
  if (a.has_secondary_text()) {
    AssignMojoField(a.secondary_text(), b->secondary_text);
  }
  if (a.has_fusebox_action()) {
    b->fusebox_action =
        fusebox_action::SyncFuseboxActionProtoToMojo(a.fusebox_action());
  }
}

// Creates a SuggestTemplateInfoPtr from an omnibox::SuggestTemplateInfo.
// Returns nullptr if we cannot handle the proto (e.g., the enum is not
// available on our side).
SuggestTemplateInfoPtr CreateSuggestTemplateInfo(
    const omnibox::SuggestTemplateInfo& suggest_template_info) {
  static_assert(
      static_cast<int32_t>(omnibox::SuggestTemplateInfo::IconType_MAX) ==
          static_cast<int32_t>(action_chips::mojom::IconType::kMaxValue),
      "IconType enum values must match between omnibox and action chips.");
  // The remote endpoint may send the icon type unknown to us.
  // When this occurs, we get the following:
  // - the default value of the enum (when the closed enum is used as in
  //   proto2)
  // - the actual (invalid) value (when the open enum is used)
  if (suggest_template_info.type_icon() ==
          omnibox::SuggestTemplateInfo::ICON_TYPE_UNSPECIFIED ||
      !omnibox::SuggestTemplateInfo::IconType_IsValid(
          suggest_template_info.type_icon())) {
    VLOG(1) << "Invalid icon type is returned from the remote endpoint.";
    return nullptr;
  }

  SuggestTemplateInfoPtr mojom_suggest_template_info =
      SuggestTemplateInfo::New();
  SyncProtoToMojo(suggest_template_info, mojom_suggest_template_info);

  return mojom_suggest_template_info;
}

// Locally generated chips explicitly request the current click behavior:
// paste the query into the Composebox without submitting it.
void SetPasteAndComposeboxOverrides(
    fusebox_action::mojom::FuseboxAction& action) {
  action.query_action_override =
      fusebox_action::mojom::QueryActionOverride::kPaste;
  action.searchbox_override =
      fusebox_action::mojom::SearchboxOverride::kComposebox;
}

// Create a recent tab chip. The chip by default (in U.S.) would look like the
// following:
// |-------------------------|
// | Ask about previous tab  |
// |  ${title of the tab}    |
// |-------------------------|
ActionChipPtr CreateRecentTabChip(TabInfoPtr tab, std::string_view suggestion) {
  ActionChipPtr chip = ActionChip::New();
  chip->suggestion = std::string();
  chip->tab = std::move(tab);
  chip->suggest_template_info = SuggestTemplateInfo::New();
  chip->suggest_template_info->type_icon = IconType::kFavicon;
  chip->suggest_template_info->primary_text =
      action_chips::mojom::FormattedString::New();
  chip->suggest_template_info->primary_text->text =
      !suggestion.empty()
          ? std::string(suggestion)
          : l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_TAB_HEADING_1);
  chip->suggest_template_info->secondary_text =
      action_chips::mojom::FormattedString::New();
  chip->suggest_template_info->secondary_text->text = chip->tab->title;
  chip->suggest_template_info->fusebox_action =
      fusebox_action::mojom::FuseboxAction::New();
  SetPasteAndComposeboxOverrides(*chip->suggest_template_info->fusebox_action);
  return chip;
}

ActionChipPtr CreateDeepSearchChip(std::string_view suggestion) {
  ActionChipPtr chip = ActionChip::New();
  chip->suggestion = std::string();
  chip->suggest_template_info = SuggestTemplateInfo::New();
  chip->suggest_template_info->type_icon = IconType::kGlobeWithSearchLoop;
  chip->suggest_template_info->primary_text =
      action_chips::mojom::FormattedString::New();
  chip->suggest_template_info->primary_text->text =
      l10n_util::GetStringUTF8(IDS_NTP_COMPOSE_DEEP_SEARCH);
  chip->suggest_template_info->secondary_text =
      action_chips::mojom::FormattedString::New();
  chip->suggest_template_info->secondary_text->text =
      !suggestion.empty()
          ? std::string(suggestion)
          : l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_DEEP_SEARCH_BODY);
  chip->suggest_template_info->fusebox_action =
      fusebox_action::mojom::FuseboxAction::New();
  chip->suggest_template_info->fusebox_action->preselected_tool =
      omnibox::TOOL_MODE_DEEP_SEARCH;
  SetPasteAndComposeboxOverrides(*chip->suggest_template_info->fusebox_action);
  return chip;
}

std::optional<ActionChipPtr> CreateDeepSearchChipIfEligible(
    std::string_view suggestion,
    const AimEligibilityService* aim_eligibility_service) {
  if (aim_eligibility_service == nullptr ||
      !aim_eligibility_service->IsDeepSearchEligible()) {
    return std::nullopt;
  }
  return CreateDeepSearchChip(suggestion);
}

ActionChipPtr CreateImageCreationChip(std::string_view suggestion) {
  ActionChipPtr chip = ActionChip::New();
  chip->suggestion = std::string();
  chip->suggest_template_info = SuggestTemplateInfo::New();
  chip->suggest_template_info->type_icon = IconType::kBanana;
  chip->suggest_template_info->primary_text =
      action_chips::mojom::FormattedString::New();
  chip->suggest_template_info->primary_text->text =
      l10n_util::GetStringUTF8(IDS_NTP_COMPOSE_CREATE_IMAGES);
  chip->suggest_template_info->secondary_text =
      action_chips::mojom::FormattedString::New();
  chip->suggest_template_info->secondary_text->text =
      !suggestion.empty()
          ? std::string(suggestion)
          : l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_CREATE_IMAGE_BODY_1);
  chip->suggest_template_info->fusebox_action =
      fusebox_action::mojom::FuseboxAction::New();
  chip->suggest_template_info->fusebox_action->preselected_tool =
      omnibox::TOOL_MODE_IMAGE_GEN;
  SetPasteAndComposeboxOverrides(*chip->suggest_template_info->fusebox_action);
  return chip;
}

std::optional<ActionChipPtr> CreateImageCreationChipIfEligible(
    std::string_view suggestion,
    const AimEligibilityService* aim_eligibility_service) {
  if (aim_eligibility_service == nullptr ||
      !aim_eligibility_service->IsCreateImagesEligible()) {
    return std::nullopt;
  }
  return CreateImageCreationChip(suggestion);
}

ActionChipPtr CreateStarterChip() {
  ActionChipPtr chip = ActionChip::New();
  chip->suggestion = std::string();
  chip->suggest_template_info = SuggestTemplateInfo::New();
  chip->suggest_template_info->type_icon = IconType::kSearchLoopWithSparkle;
  chip->suggest_template_info->primary_text =
      action_chips::mojom::FormattedString::New();
  chip->suggest_template_info->primary_text->text =
      l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_STARTER_HEADING);
  chip->suggest_template_info->secondary_text =
      action_chips::mojom::FormattedString::New();
  chip->suggest_template_info->secondary_text->text =
      l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_STARTER_BODY);
  chip->suggest_template_info->fusebox_action =
      fusebox_action::mojom::FuseboxAction::New();
  chip->suggest_template_info->fusebox_action->preferred_inventory =
      omnibox::SUGGEST_INVENTORY_AIM_CONVERSATION_STARTERS;
  SetPasteAndComposeboxOverrides(*chip->suggest_template_info->fusebox_action);
  return chip;
}

std::optional<ActionChipPtr> CreateStarterChipIfEligible(
    std::string_view suggestion,
    const AimEligibilityService* aim_eligibility_service) {
  if (base::FeatureList::IsEnabled(ntp_features::kNtpStarterChip) &&
      aim_eligibility_service &&
      aim_eligibility_service->IsCreateImagesEligible()) {
    return CreateStarterChip();
  }
  return std::nullopt;
}

ActionChipPtr CreateCanvasChip(std::string_view suggestion) {
  ActionChipPtr chip = ActionChip::New();
  chip->suggestion = std::string();
  chip->suggest_template_info = SuggestTemplateInfo::New();
  chip->suggest_template_info->type_icon = IconType::kDraftSpark;
  chip->suggest_template_info->primary_text =
      action_chips::mojom::FormattedString::New();
  chip->suggest_template_info->primary_text->text =
      l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_CANVAS_HEADING);
  chip->suggest_template_info->secondary_text =
      action_chips::mojom::FormattedString::New();
  chip->suggest_template_info->secondary_text->text =
      !suggestion.empty()
          ? std::string(suggestion)
          : l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_CANVAS_BODY);
  chip->suggest_template_info->fusebox_action =
      fusebox_action::mojom::FuseboxAction::New();
  chip->suggest_template_info->fusebox_action->preselected_tool =
      omnibox::TOOL_MODE_CANVAS;
  SetPasteAndComposeboxOverrides(*chip->suggest_template_info->fusebox_action);
  return chip;
}

std::optional<ActionChipPtr> CreateCanvasChipIfEligible(
    std::string_view suggestion,
    const AimEligibilityService* aim_eligibility_service) {
  if (!base::FeatureList::IsEnabled(ntp_features::kNtpNextCanvasChip) ||
      aim_eligibility_service == nullptr ||
      !aim_eligibility_service->IsCanvasEligible()) {
    return std::nullopt;
  }
  return CreateCanvasChip(suggestion);
}

ActionChipPtr CreateBrainstormChip() {
  ActionChipPtr chip = ActionChip::New();
  chip->suggestion = std::string();
  chip->suggest_template_info = SuggestTemplateInfo::New();
  chip->suggest_template_info->type_icon = IconType::kLightbulb;
  chip->suggest_template_info->primary_text =
      action_chips::mojom::FormattedString::New();
  chip->suggest_template_info->primary_text->text =
      l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_BRAINSTORM_HEADING);
  chip->suggest_template_info->fusebox_action =
      fusebox_action::mojom::FuseboxAction::New();
  chip->suggest_template_info->fusebox_action->preferred_inventory =
      omnibox::SUGGEST_INVENTORY_BRAINSTORM;
  SetPasteAndComposeboxOverrides(*chip->suggest_template_info->fusebox_action);
  return chip;
}

std::optional<ActionChipPtr> CreateBrainstormChipIfEligible(
    std::string_view suggestion,
    const AimEligibilityService* aim_eligibility_service) {
  if (!IsScaledChipsEnabled()) {
    return std::nullopt;
  }
  return CreateBrainstormChip();
}

ActionChipPtr CreateLearnChip() {
  ActionChipPtr chip = ActionChip::New();
  chip->suggestion = std::string();
  chip->suggest_template_info = SuggestTemplateInfo::New();
  chip->suggest_template_info->type_icon = IconType::kSchool;
  chip->suggest_template_info->primary_text =
      action_chips::mojom::FormattedString::New();
  chip->suggest_template_info->primary_text->text =
      l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_HELP_ME_LEARN_HEADING);
  chip->suggest_template_info->fusebox_action =
      fusebox_action::mojom::FuseboxAction::New();
  chip->suggest_template_info->fusebox_action->preferred_inventory =
      omnibox::SUGGEST_INVENTORY_HELP_ME_LEARN;
  SetPasteAndComposeboxOverrides(*chip->suggest_template_info->fusebox_action);
  return chip;
}

std::optional<ActionChipPtr> CreateLearnChipIfEligible(
    std::string_view suggestion,
    const AimEligibilityService* aim_eligibility_service) {
  if (!IsScaledChipsEnabled()) {
    return std::nullopt;
  }
  return CreateLearnChip();
}

ActionChipPtr CreateWriteChip() {
  ActionChipPtr chip = ActionChip::New();
  chip->suggestion = std::string();
  chip->suggest_template_info = SuggestTemplateInfo::New();
  chip->suggest_template_info->type_icon = IconType::kInkPen;
  chip->suggest_template_info->primary_text =
      action_chips::mojom::FormattedString::New();
  chip->suggest_template_info->primary_text->text =
      l10n_util::GetStringUTF8(IDS_NTP_ACTION_CHIP_WRITE_EDIT_HEADING);
  chip->suggest_template_info->fusebox_action =
      fusebox_action::mojom::FuseboxAction::New();
  chip->suggest_template_info->fusebox_action->preferred_inventory =
      omnibox::SUGGEST_INVENTORY_WRITE_OR_EDIT;
  SetPasteAndComposeboxOverrides(*chip->suggest_template_info->fusebox_action);
  return chip;
}

std::optional<ActionChipPtr> CreateWriteChipIfEligible(
    std::string_view suggestion,
    const AimEligibilityService* aim_eligibility_service) {
  if (!IsScaledChipsEnabled()) {
    return std::nullopt;
  }
  return CreateWriteChip();
}

ActionChipPtr CreateAddImageChip() {
  ActionChipPtr chip = ActionChip::New();
  chip->suggestion = "";
  chip->suggest_template_info = SuggestTemplateInfo::New();
  chip->suggest_template_info->type_icon = IconType::kAttachFile;
  chip->suggest_template_info->primary_text =
      action_chips::mojom::FormattedString::New();
  chip->suggest_template_info->primary_text->text = "Add Image";
  chip->suggest_template_info->fusebox_action =
      fusebox_action::mojom::FuseboxAction::New();
  chip->suggest_template_info->fusebox_action->preselected_input_source =
      fusebox_action::mojom::InputSource::kInputSourceGallery;
  return chip;
}

std::optional<ActionChipPtr> CreateAddImageChipIfEligible(
    std::string_view suggestion,
    const AimEligibilityService* aim_eligibility_service) {
  if (!(base::FeatureList::IsEnabled(
            ntp_features::kNtpScaledActionChipsSmall) &&
        ntp_features::kNtpScaledActionChipsSmallInTestMode.Get())) {
    return std::nullopt;
  }
  return CreateAddImageChip();
}

std::vector<omnibox::ToolMode> GetAllowedTools(
    const AimEligibilityService* aim_eligibility_service) {
  std::vector<omnibox::ToolMode> tools;
  if (aim_eligibility_service == nullptr) {
    return tools;
  }
  const omnibox::SearchboxConfig* searchbox_config =
      aim_eligibility_service->GetSearchboxConfig();
  for (const auto& tool_config : searchbox_config->tool_configs()) {
    tools.push_back(tool_config.tool());
  }
  return tools;
}

std::vector<omnibox::InputType> GetAllowedInputs(
    const AimEligibilityService* aim_eligibility_service) {
  std::vector<omnibox::InputType> inputs;
  if (aim_eligibility_service == nullptr) {
    return inputs;
  }
  const omnibox::SearchboxConfig* searchbox_config =
      aim_eligibility_service->GetSearchboxConfig();
  for (const auto& input_type_config : searchbox_config->input_type_configs()) {
    inputs.push_back(input_type_config.input_type());
  }
  return inputs;
}

TabInfoPtr CreateTabInfo(const TabIdGenerator& tab_id_generator,
                         const TabInterface& tab) {
  TabInfoPtr tab_info = TabInfo::New();
  tab_info->tab_id = tab_id_generator.GenerateTabHandleId(&tab);
  content::WebContents& contents = *tab.GetContents();
  tab_info->title = base::UTF16ToUTF8(contents.GetTitle());
  tab_info->url = contents.GetLastCommittedURL();
  tab_info->last_active_time = contents.GetLastActiveTime();
  return tab_info;
}

TabInfoPtr CreateTabInfo(const TabIdGenerator& tab_id_generator,
                         base::optional_ref<const TabInterface> tab) {
  return tab.has_value() ? CreateTabInfo(tab_id_generator, *tab) : nullptr;
}

std::vector<ActionChipPtr> CreateChipsForSteadyState(
    TabInfoPtr tab,
    const AimEligibilityService* aim_eligibility_service) {
  std::vector<ActionChipPtr> chips;

  if (!tab.is_null() &&
      ntp_features::kNtpNextShowStaticRecentTabChipParam.Get()) {
    chips.push_back(CreateRecentTabChip(std::move(tab), /*suggestion=*/""));
  }

  using GeneratorFn = const base::FunctionRef<std::optional<ActionChipPtr>(
      std::string_view, const AimEligibilityService*)>;

  // Scaled action chips.
  static const GeneratorFn kScaledGenerators[] = {
      &CreateBrainstormChipIfEligible,
      &CreateLearnChipIfEligible,
      &CreateWriteChipIfEligible,
      // TODO(crbug.com/537040757): Remove from here, only adding for test
      // purposes until the server sends new chip types.
      &CreateAddImageChipIfEligible,
  };

  // Pre-scaled fallback chips with starter chip and canvas tool.
  static const GeneratorFn kNewGenerators[] = {
      &CreateStarterChipIfEligible,
      &CreateImageCreationChipIfEligible,
      &CreateCanvasChipIfEligible,
      &CreateDeepSearchChipIfEligible,
  };

  // Legacy baseline fallback chips.
  static const GeneratorFn kOldGenerators[] = {
      &CreateDeepSearchChipIfEligible,
      &CreateImageCreationChipIfEligible,
  };

  base::span<const GeneratorFn> generators;
  if (IsScaledChipsEnabled()) {
    generators = kScaledGenerators;
  } else if (base::FeatureList::IsEnabled(ntp_features::kNtpNextCanvasChip) ||
             base::FeatureList::IsEnabled(ntp_features::kNtpStarterChip)) {
    generators = kNewGenerators;
  } else {
    generators = kOldGenerators;
  }

  const size_t max_num_chips = GetMaxNumChips();
  for (const GeneratorFn generator : generators) {
    if (chips.size() >= max_num_chips) {
      break;
    }
    if (std::optional<ActionChipPtr> chip =
            generator(/*suggestion=*/"", aim_eligibility_service)) {
      chips.push_back(std::move(*chip));
    }
  }
  return chips;
}

struct TitleAndUrl {
  std::optional<std::u16string> title;
  std::optional<GURL> url;
};

TitleAndUrl GetTitleAndUrl(base::optional_ref<const TabInterface> tab) {
  if (!tab.has_value()) {
    return {};
  }
  content::WebContents& contents = *tab->GetContents();
  return {
      .title = contents.GetTitle(),
      .url = contents.GetLastCommittedURL(),
  };
}
struct ParsedActionChipData {
  SuggestTemplateInfoPtr suggest_template_info;
  omnibox::GroupId group_id;
};

std::optional<ParsedActionChipData> ExtractActionChipData(
    const SearchSuggestionParser::SuggestResult& suggestion,
    std::optional<const omnibox::PageVertical> page_vertical) {
  if (suggestion.suggest_type() != omnibox::SuggestType::TYPE_FUSEBOX_ACTION) {
    VLOG(1) << "Skipping a suggestion whose suggest type was: "
            << suggestion.suggest_type();
    return std::nullopt;
  }

  if (!suggestion.suggestion_group_id().has_value() ||
      suggestion.suggestion_group_id().value() == omnibox::GROUP_INVALID) {
    VLOG(1) << "A suggestion did not have a valid group ID. Its "
               "match_contents was: "
            << suggestion.match_contents();
    return std::nullopt;
  }
  if (!suggestion.suggest_template_info().has_value()) {
    VLOG(1) << "A suggestion did not have a SuggestTemplateInfo. Its "
               "match_contents was: "
            << suggestion.match_contents();
    return std::nullopt;
  }
  SuggestTemplateInfoPtr mojom_suggest_template_info =
      CreateSuggestTemplateInfo(*suggestion.suggest_template_info());
  if (mojom_suggest_template_info.is_null()) {
    return std::nullopt;
  }
  omnibox::GroupId group_id = suggestion.suggestion_group_id().value();

  return ParsedActionChipData{std::move(mojom_suggest_template_info), group_id};
}

}  // namespace

ActionChipsGeneratorImpl::ActionChipsGeneratorImpl(Profile* profile)
    : tab_id_generator_(TabIdGeneratorImpl::Get()),
      aim_eligibility_service_(
          AimEligibilityServiceFactory::GetForProfile(profile)),
      client_(std::make_unique<ChromeAutocompleteProviderClient>(profile)),
      remote_suggestions_service_simple_(
          std::make_unique<RemoteSuggestionsServiceSimpleImpl>(client_.get())) {
}

ActionChipsGeneratorImpl::ActionChipsGeneratorImpl(
    const TabIdGenerator* tab_id_generator,
    const AimEligibilityService* aim_eligibility_service,
    std::unique_ptr<AutocompleteProviderClient> client,
    std::unique_ptr<RemoteSuggestionsServiceSimple>
        remote_suggestions_service_simple)
    : tab_id_generator_(tab_id_generator),
      aim_eligibility_service_(aim_eligibility_service),
      client_(std::move(client)),
      remote_suggestions_service_simple_(
          std::move(remote_suggestions_service_simple)) {}

ActionChipsGeneratorImpl::~ActionChipsGeneratorImpl() = default;

void ActionChipsGeneratorImpl::GenerateActionChips(
    base::optional_ref<const TabInterface> tab,
    base::OnceCallback<void(std::vector<ActionChipPtr>)> callback) {
  // Cancel the existing chips generation by destructing the
  // loader.
  loader_.reset();

  if (ntp_features::kNtpNextShowStaticTextParam.Get() ||
      ntp_features::kNtpScaledActionChipsShowFallback.Get()) {
    std::move(callback).Run(CreateChipsForSteadyState(
        CreateTabInfo(*tab_id_generator_, tab), aim_eligibility_service_));
    return;
  }

  GenerateActionChipsFromNewEndpoint(
      client_->IsPersonalizedUrlDataCollectionActive() ? tab : std::nullopt,
      std::move(callback));
}

void ActionChipsGeneratorImpl::GenerateActionChipsFromNewEndpoint(
    base::optional_ref<const TabInterface> tab,
    base::OnceCallback<void(std::vector<ActionChipPtr>)> callback) {
  std::optional<omnibox::PageVertical> page_vertical;

  auto [title, url] = GetTitleAndUrl(tab);
  loader_ = remote_suggestions_service_simple_->GetActionChipSuggestions(
      title, url, GetAllowedTools(aim_eligibility_service_),
      GetAllowedInputs(aim_eligibility_service_), page_vertical,
      base::BindOnce(
          &ActionChipsGeneratorImpl::GenerateActionChipsFromRemoteResponse,
          this->weak_factory_.GetWeakPtr(),
          CreateTabInfo(*tab_id_generator_, tab), page_vertical,
          std::move(callback)));
}

void ActionChipsGeneratorImpl::GenerateActionChipsFromRemoteResponse(
    TabInfoPtr tab,
    std::optional<const omnibox::PageVertical> page_vertical,
    base::OnceCallback<void(std::vector<ActionChipPtr>)> callback,
    RemoteSuggestionsServiceSimple::ActionChipSuggestionsResult&& result) {
  RecordActionChipsRequestStatus(result);

  std::vector<ActionChipPtr> chips;
  if (result.has_value()) {
    const size_t max_num_chips = GetMaxNumChips();
    for (const auto& suggestion : *result) {
      if (chips.size() >= max_num_chips) {
        break;
      }
      std::optional<ParsedActionChipData> parsed_data =
          ExtractActionChipData(suggestion, page_vertical);
      if (!parsed_data.has_value()) {
        continue;
      }

      ActionChipPtr chip = ActionChip::New();
      chip->suggest_template_info =
          std::move(parsed_data->suggest_template_info);

      chip->suggestion = base::UTF16ToUTF8(suggestion.suggestion());
      if (parsed_data->group_id ==
          omnibox::GROUP_AI_MODE_CONTEXTUAL_SEARCH_ACTION) {
        if (tab) {
          chip->tab = tab->Clone();
        }
      }
      chips.push_back(std::move(chip));
    }
  }

  // Fall back to steady-state chips if the remote response did not yield any
  // valid chips.
  if (chips.empty()) {
    std::move(callback).Run(
        CreateChipsForSteadyState(std::move(tab), aim_eligibility_service_));
    return;
  }
  std::move(callback).Run(std::move(chips));
}
