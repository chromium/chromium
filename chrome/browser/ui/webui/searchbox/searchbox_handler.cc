// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/searchbox_handler.h"

#include "base/base64.h"
#include "base/base64url.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_feature_configs.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/variations_client.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_ui_data_source.h"
#include "third_party/omnibox_proto/answer_data.pb.h"
#include "third_party/omnibox_proto/answer_type.pb.h"
#include "third_party/omnibox_proto/rich_answer_template.pb.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// TODO(niharm): convert back to constexpr char[] once feature is cleaned up
const char* kAnswerCurrencyIconResourceName =
    "//resources/cr_components/searchbox/icons/currency.svg";
constexpr char kAnswerDefaultIconResourceName[] =
    "//resources/cr_components/searchbox/icons/default.svg";
const char* kAnswerDictionaryIconResourceName =
    "//resources/cr_components/searchbox/icons/definition.svg";
const char* kAnswerFinanceIconResourceName =
    "//resources/cr_components/searchbox/icons/finance.svg";
const char* kAnswerSunriseIconResourceName =
    "//resources/cr_components/searchbox/icons/sunrise.svg";
const char* kAnswerTranslationIconResourceName =
    "//resources/cr_components/searchbox/icons/translation.svg";
const char* kAnswerWhenIsIconResourceName =
    "//resources/cr_components/searchbox/icons/when_is.svg";
const char* kBookmarkIconResourceName = "//resources/images/icon_bookmark.svg";
const char* kCalculatorIconResourceName =
    "//resources/cr_components/searchbox/icons/calculator.svg";
const char* kChromeProductIconResourceName =
    "//resources/cr_components/searchbox/icons/chrome_product.svg";
const char* kClockIconResourceName = "//resources/images/icon_clock.svg";
const char* kDinoIconResourceName =
    "//resources/cr_components/searchbox/icons/dino.svg";
constexpr char kDriveDocsIconResourceName[] =
    "//resources/cr_components/searchbox/icons/drive_docs.svg";
constexpr char kDriveFolderIconResourceName[] =
    "//resources/cr_components/searchbox/icons/drive_folder.svg";
constexpr char kDriveFormIconResourceName[] =
    "//resources/cr_components/searchbox/icons/drive_form.svg";
constexpr char kDriveImageIconResourceName[] =
    "//resources/cr_components/searchbox/icons/drive_image.svg";
constexpr char kDriveLogoIconResourceName[] =
    "//resources/cr_components/searchbox/icons/drive_logo.svg";
constexpr char kDrivePdfIconResourceName[] =
    "//resources/cr_components/searchbox/icons/drive_pdf.svg";
constexpr char kDriveSheetsIconResourceName[] =
    "//resources/cr_components/searchbox/icons/drive_sheets.svg";
constexpr char kDriveSlidesIconResourceName[] =
    "//resources/cr_components/searchbox/icons/drive_slides.svg";
constexpr char kDriveVideoIconResourceName[] =
    "//resources/cr_components/searchbox/icons/drive_video.svg";
constexpr char kExtensionAppIconResourceName[] =
    "//resources/cr_components/searchbox/icons/extension_app.svg";
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr char kGoogleCalendarIconResourceName[] =
    "//resources/cr_components/searchbox/icons/calendar.svg";
const char* kGoogleGIconResourceName =
    "//resources/cr_components/searchbox/icons/google_g.svg";
constexpr char kGoogleKeepNoteIconResourceName[] =
    "//resources/cr_components/searchbox/icons/note.svg";
constexpr char kGoogleSitesIconResourceName[] =
    "//resources/cr_components/searchbox/icons/sites.svg";
#endif
const char* kHistoryIconResourceName = "//resources/images/icon_history.svg";
const char* kIncognitoIconResourceName =
    "//resources/cr_components/searchbox/icons/incognito.svg";
const char* kJourneysIconResourceName =
    "//resources/cr_components/searchbox/icons/journeys.svg";
const char* kPageIconResourceName =
    "//resources/cr_components/searchbox/icons/page.svg";
const char* kPedalsIconResourceName = "//theme/current-channel-logo";
const char* kSearchIconResourceName = "//resources/images/icon_search.svg";
const char* kSparkIconResourceName =
    "//resources/cr_components/searchbox/icons/spark.svg";
const char* kStarActiveIconResourceName =
    "//resources/cr_components/searchbox/icons/star_active.svg";
const char* kTabIconResourceName =
    "//resources/cr_components/searchbox/icons/tab.svg";
const char* kTrendingUpIconResourceName =
    "//resources/cr_components/searchbox/icons/trending_up.svg";

#if BUILDFLAG(IS_MAC)
const char* kMacShareIconResourceName =
    "//resources/cr_components/searchbox/icons/mac_share.svg";
#elif BUILDFLAG(IS_WIN)
const char* kWinShareIconResourceName =
    "//resources/cr_components/searchbox/icons/win_share.svg";
#elif BUILDFLAG(IS_LINUX)
const char* kLinuxShareIconResourceName =
    "//resources/cr_components/searchbox/icons/share.svg";
#else
const char* kShareIconResourceName =
    "//resources/cr_components/searchbox/icons/share.svg";
#endif

static void DefineChromeRefreshRealboxIcons() {
  kAnswerCurrencyIconResourceName =
      "//resources/cr_components/searchbox/icons/currency_cr23.svg";
  kAnswerDictionaryIconResourceName =
      "//resources/cr_components/searchbox/icons/definition_cr23.svg";
  kAnswerFinanceIconResourceName =
      "//resources/cr_components/searchbox/icons/finance_cr23.svg";
  kAnswerSunriseIconResourceName =
      "//resources/cr_components/searchbox/icons/sunrise_cr23.svg";
  kAnswerTranslationIconResourceName =
      "//resources/cr_components/searchbox/icons/translation_cr23.svg";
  kAnswerWhenIsIconResourceName =
      "//resources/cr_components/searchbox/icons/when_is_cr23.svg";
  kBookmarkIconResourceName =
      "//resources/cr_components/searchbox/icons/bookmark_cr23.svg";
  kCalculatorIconResourceName =
      "//resources/cr_components/searchbox/icons/calculator_cr23.svg";
  kChromeProductIconResourceName =
      "//resources/cr_components/searchbox/icons/chrome_product_cr23.svg";
  kClockIconResourceName =
      "//resources/cr_components/searchbox/icons/clock_cr23.svg";
  kDinoIconResourceName =
      "//resources/cr_components/searchbox/icons/dino_cr23.svg";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  kGoogleGIconResourceName =
      "//resources/cr_components/searchbox/icons/google_g_cr23.svg";
#endif

  kHistoryIconResourceName =
      "//resources/cr_components/searchbox/icons/history_cr23.svg";
  kIncognitoIconResourceName =
      "//resources/cr_components/searchbox/icons/incognito_cr23.svg";
  kJourneysIconResourceName =
      "//resources/cr_components/searchbox/icons/journeys_cr23.svg";
  kPageIconResourceName =
      "//resources/cr_components/searchbox/icons/page_cr23.svg";
  kPedalsIconResourceName =
      "//resources/cr_components/searchbox/icons/chrome_product_cr23.svg";
  kSearchIconResourceName =
      "//resources/cr_components/searchbox/icons/search_cr23.svg";
  kTabIconResourceName =
      "//resources/cr_components/searchbox/icons/tab_cr23.svg";
  kTrendingUpIconResourceName =
      "//resources/cr_components/searchbox/icons/trending_up_cr23.svg";

#if BUILDFLAG(IS_MAC)
  kMacShareIconResourceName =
      "//resources/cr_components/searchbox/icons/mac_share_cr23.svg";
#elif BUILDFLAG(IS_WIN)
  kWinShareIconResourceName =
      "//resources/cr_components/searchbox/icons/win_share_cr23.svg";
#elif BUILDFLAG(IS_LINUX)
  kLinuxShareIconResourceName =
      "//resources/cr_components/searchbox/icons/share_cr23.svg";
#else
  kShareIconResourceName =
      "//resources/cr_components/searchbox/icons/share_cr23.svg";
#endif
}

std::u16string GetAdditionalA11yMessage(
    const AutocompleteMatch& match,
    searchbox::mojom::SelectionLineState state) {
  switch (state) {
    case searchbox::mojom::SelectionLineState::kNormal: {
      if (match.has_tab_match.value_or(false) &&
          base::FeatureList::IsEnabled(omnibox::kNtpRealboxPedals)) {
        return l10n_util::GetStringUTF16(IDS_ACC_TAB_SWITCH_SUFFIX);
      }
      const OmniboxAction* action = match.GetActionAt(0u);
      if (action) {
        return action->GetLabelStrings().accessibility_suffix;
      }
      if (match.SupportsDeletion()) {
        return l10n_util::GetStringUTF16(IDS_ACC_REMOVE_SUGGESTION_SUFFIX);
      }
      break;
    }
    case searchbox::mojom::SelectionLineState::kFocusedButtonRemoveSuggestion:
      return l10n_util::GetStringUTF16(
          IDS_ACC_REMOVE_SUGGESTION_FOCUSED_PREFIX);
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  return std::u16string();
}

std::optional<std::u16string> GetAdditionalText(
    const SuggestionAnswer::ImageLine& line) {
  if (line.additional_text()) {
    const auto additional_text = line.additional_text()->text();
    if (!additional_text.empty()) {
      return additional_text;
    }
  }
  return std::nullopt;
}

std::u16string ImageLineToString16(const SuggestionAnswer::ImageLine& line) {
  std::vector<std::u16string> text;
  for (const auto& text_field : line.text_fields()) {
    text.push_back(text_field.text());
  }
  const auto& additional_text = GetAdditionalText(line);
  if (additional_text) {
    text.push_back(additional_text.value());
  }
  // TODO(crbug.com/40149768): Use placeholders or a l10n-friendly way to
  // construct this string instead of concatenation. This currently only happens
  // for stock ticker symbols.
  return base::JoinString(text, u" ");
}

bool MatchHasSideTypeAndRenderType(
    const AutocompleteMatch& match,
    omnibox::GroupConfig_SideType side_type,
    omnibox::GroupConfig_RenderType render_type,
    const omnibox::GroupConfigMap& suggestion_groups_map) {
  omnibox::GroupId group_id =
      match.suggestion_group_id.value_or(omnibox::GROUP_INVALID);
  return base::Contains(suggestion_groups_map, group_id) &&
         suggestion_groups_map.at(group_id).side_type() == side_type &&
         suggestion_groups_map.at(group_id).render_type() == render_type;
}

std::vector<searchbox::mojom::AutocompleteMatchPtr> CreateAutocompleteMatches(
    const AutocompleteResult& result,
    bookmarks::BookmarkModel* bookmark_model,
    const omnibox::GroupConfigMap& suggestion_groups_map,
    const TemplateURLService* turl_service) {
  std::vector<searchbox::mojom::AutocompleteMatchPtr> matches;
  int line = 0;
  for (const AutocompleteMatch& match : result) {
    // Skip the primary column horizontal matches. This check guards against
    // this unexpected scenario as the UI expects the primary column matches to
    // be vertical ones.
    if (MatchHasSideTypeAndRenderType(
            match, omnibox::GroupConfig_SideType_DEFAULT_PRIMARY,
            omnibox::GroupConfig_RenderType_HORIZONTAL,
            suggestion_groups_map)) {
      continue;
    }

    // Skip the secondary column horizontal matches that are not entities or do
    // not have images. This check guards against this unexpected scenario as
    // the UI expects the secondary column horizontal matches to be entity
    // suggestions with images.
    if (MatchHasSideTypeAndRenderType(
            match, omnibox::GroupConfig_SideType_SECONDARY,
            omnibox::GroupConfig_RenderType_HORIZONTAL,
            suggestion_groups_map) &&
        (match.type != AutocompleteMatchType::SEARCH_SUGGEST_ENTITY ||
         !match.image_url.is_valid())) {
      continue;
    }

    searchbox::mojom::AutocompleteMatchPtr mojom_match =
        searchbox::mojom::AutocompleteMatch::New();
    mojom_match->allowed_to_be_default_match =
        match.allowed_to_be_default_match;
    mojom_match->contents = match.contents;
    for (const auto& contents_class : match.contents_class) {
      mojom_match->contents_class.push_back(
          searchbox::mojom::ACMatchClassification::New(contents_class.offset,
                                                       contents_class.style));
    }
    mojom_match->description = match.description;
    for (const auto& description_class : match.description_class) {
      mojom_match->description_class.push_back(
          searchbox::mojom::ACMatchClassification::New(
              description_class.offset, description_class.style));
    }
    mojom_match->destination_url = match.destination_url;
    mojom_match->suggestion_group_id =
        match.suggestion_group_id.value_or(omnibox::GROUP_INVALID);
    const bool is_bookmarked =
        bookmark_model->IsBookmarked(match.destination_url);
    // For starter pack suggestions, use template url to generate proper vector
    // icon.
    const TemplateURL* turl = match.associated_keyword
                                  ? turl_service->GetTemplateURLForKeyword(
                                        match.associated_keyword->keyword)
                                  : nullptr;
    mojom_match->icon_url =
        SearchboxHandler::AutocompleteMatchVectorIconToResourceName(
            match.GetVectorIcon(is_bookmarked, turl));
    mojom_match->image_dominant_color = match.image_dominant_color;
    mojom_match->image_url = match.image_url.spec();
    mojom_match->fill_into_edit = match.fill_into_edit;
    mojom_match->inline_autocompletion = match.inline_autocompletion;
    mojom_match->is_search_type = AutocompleteMatch::IsSearchType(match.type);
    mojom_match->swap_contents_and_description =
        match.swap_contents_and_description;
    mojom_match->type = AutocompleteMatchType::ToString(match.type);
    mojom_match->supports_deletion = match.SupportsDeletion();
    if (omnibox_feature_configs::SuggestionAnswerMigration::Get().enabled &&
        match.answer_template.has_value()) {
      const omnibox::AnswerData& answer_data =
          match.answer_template->answers(0);
      const omnibox::FormattedString& headline = answer_data.headline();
      std::u16string headline_substr;
      if (headline.fragments_size() > 0) {
        const std::string& headline_text = headline.text();
        // Grab the substring of headline starting after the first fragment text
        // ends. Not making use of the first fragment because it contains the
        // same data as `match.contents` but with HTML tags.
        headline_substr = base::UTF8ToUTF16(headline_text.substr(
            headline.fragments(0).text().size(),
            headline_text.size() - headline.fragments(0).text().size()));
      }

      const auto& subhead_text =
          base::UTF8ToUTF16(answer_data.subhead().text());
      // Reusing SuggestionAnswer because `headline` and `subhead` are
      // equivalent to `first_line` and `second_line`.
      mojom_match->answer = searchbox::mojom::SuggestionAnswer::New(
          headline_substr.empty()
              ? match.contents
              : base::JoinString({match.contents, headline_substr}, u" "),
          subhead_text);
      mojom_match->image_url = answer_data.image().url();
      mojom_match->is_weather_answer_suggestion =
          match.answer_type == omnibox::ANSWER_TYPE_WEATHER;
    } else if (match.answer.has_value()) {
      const auto& additional_text =
          GetAdditionalText(match.answer->first_line());
      mojom_match->answer = searchbox::mojom::SuggestionAnswer::New(
          additional_text ? base::JoinString(
                                {match.contents, additional_text.value()}, u" ")
                          : match.contents,
          ImageLineToString16(match.answer->second_line()));
      mojom_match->image_url = match.ImageUrl().spec();
      mojom_match->is_weather_answer_suggestion =
          match.answer_type == omnibox::ANSWER_TYPE_WEATHER;
    }
    mojom_match->is_rich_suggestion =
        !mojom_match->image_url.empty() ||
        match.type == AutocompleteMatchType::CALCULATOR ||
        match.answer_type != omnibox::ANSWER_TYPE_UNSPECIFIED;
    if (base::FeatureList::IsEnabled(omnibox::kNtpRealboxPedals)) {
      for (const auto& action : match.actions) {
        const OmniboxAction::LabelStrings& label_strings =
            action->GetLabelStrings();
        mojom_match->actions.emplace_back(searchbox::mojom::Action::New(
            label_strings.accessibility_hint, label_strings.hint,
            label_strings.suggestion_contents,
            SearchboxHandler::ActionVectorIconToResourceName(
                action->GetVectorIcon())));
      }
    }
    mojom_match->a11y_label = AutocompleteMatchType::ToAccessibilityLabel(
        match, match.contents, line, 0,
        GetAdditionalA11yMessage(
            match, searchbox::mojom::SelectionLineState::kNormal));

    mojom_match->remove_button_a11y_label =
        AutocompleteMatchType::ToAccessibilityLabel(
            match, match.contents, line, 0,
            GetAdditionalA11yMessage(match,
                                     searchbox::mojom::SelectionLineState::
                                         kFocusedButtonRemoveSuggestion));

    mojom_match->tail_suggest_common_prefix = match.tail_suggest_common_prefix;

    matches.push_back(std::move(mojom_match));
    line++;
  }
  return matches;
}

std::string GetBase64UrlVariations(Profile* profile) {
  variations::VariationsClient* provider = profile->GetVariationsClient();

  variations::mojom::VariationsHeadersPtr headers =
      provider->GetVariationsHeaders();
  if (headers.is_null()) {
    return std::string();
  }
  const std::string variations_base64 = headers->headers_map.at(
      variations::mojom::GoogleWebVisibility::FIRST_PARTY);

  // Variations headers are base64 encoded, however, we're attaching the value
  // to a URL query parameter so they need to be base64url encoded.
  std::string variations_decoded;
  base::Base64Decode(variations_base64, &variations_decoded);

  std::string variations_base64url;
  base::Base64UrlEncode(variations_decoded,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &variations_base64url);

  return variations_base64url;
}

base::flat_map<int32_t, searchbox::mojom::SuggestionGroupPtr>
CreateSuggestionGroupsMap(
    const AutocompleteResult& result,
    const PrefService* prefs,
    const omnibox::GroupConfigMap& suggestion_groups_map) {
  base::flat_map<int32_t, searchbox::mojom::SuggestionGroupPtr> result_map;
  for (const auto& pair : suggestion_groups_map) {
    searchbox::mojom::SuggestionGroupPtr suggestion_group =
        searchbox::mojom::SuggestionGroup::New();
    suggestion_group->header = base::UTF8ToUTF16(pair.second.header_text());
    suggestion_group->side_type =
        static_cast<searchbox::mojom::SideType>(pair.second.side_type());
    suggestion_group->render_type =
        static_cast<searchbox::mojom::RenderType>(pair.second.render_type());
    suggestion_group->hidden =
        result.IsSuggestionGroupHidden(prefs, pair.first);
    suggestion_group->show_group_a11y_label = l10n_util::GetStringFUTF16(
        IDS_ACC_HEADER_SHOW_SUGGESTIONS_BUTTON, suggestion_group->header);
    suggestion_group->hide_group_a11y_label = l10n_util::GetStringFUTF16(
        IDS_ACC_HEADER_HIDE_SUGGESTIONS_BUTTON, suggestion_group->header);

    result_map.emplace(static_cast<int>(pair.first),
                       std::move(suggestion_group));
  }
  return result_map;
}

searchbox::mojom::AutocompleteResultPtr CreateAutocompleteResult(
    const std::u16string& input,
    const AutocompleteResult& result,
    bookmarks::BookmarkModel* bookmark_model,
    const PrefService* prefs,
    const TemplateURLService* turl_service) {
  return searchbox::mojom::AutocompleteResult::New(
      input,
      CreateSuggestionGroupsMap(result, prefs, result.suggestion_groups_map()),
      CreateAutocompleteMatches(result, bookmark_model,
                                result.suggestion_groups_map(), turl_service));
}

}  // namespace

// static
void SearchboxHandler::SetupWebUIDataSource(content::WebUIDataSource* source,
                                            Profile* profile,
                                            bool enable_voice_search,
                                            bool enable_lens_search) {
  // Embedders which are served from chrome-untrusted:// URLs should override
  // this to false. The chrome.timeTicks capability that the metrics reporter
  // depends on is not defined in chrome-untrusted environments and attempting
  // to use it will lead to a renderer process crash. See
  // http://g/chrome-webui/haW6I9yt-uA/38ckX-aGAgAJ for details.
  source->AddBoolean("reportMetrics", true);

  // The lens searchboxes overrides this to true to adjust various color and
  // layout options.
  source->AddBoolean("isLensSearchbox", false);

  static constexpr webui::LocalizedString kStrings[] = {
      {"hideSuggestions", IDS_TOOLTIP_HEADER_HIDE_SUGGESTIONS_BUTTON},
      {"lensSearchButtonLabel", IDS_TOOLTIP_LENS_SEARCH},
      {"searchboxSeparator", IDS_AUTOCOMPLETE_MATCH_DESCRIPTION_SEPARATOR},
      {"removeSuggestion", IDS_OMNIBOX_REMOVE_SUGGESTION},
      {"searchBoxHint", IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_MD},
      {"searchBoxHintMultimodal", IDS_GOOGLE_SEARCH_BOX_EMPTY_HINT_MULTIMODAL},
      {"searchboxThumbnailLabel",
       IDS_GOOGLE_SEARCH_BOX_MULTIMODAL_IMAGE_THUMBNAIL},
      {"showSuggestions", IDS_TOOLTIP_HEADER_SHOW_SUGGESTIONS_BUTTON},
      {"voiceSearchButtonLabel", IDS_TOOLTIP_MIC_SEARCH}};
  source->AddLocalizedStrings(kStrings);

  source->AddBoolean(
      "searchboxMatchSearchboxTheme",
      base::FeatureList::IsEnabled(ntp_features::kRealboxMatchSearchboxTheme));

  bool redesigned_modules_enabled =
      base::FeatureList::IsEnabled(ntp_features::kNtpModulesRedesigned);
  source->AddString("searchboxWidthBehavior",
                    redesigned_modules_enabled ? "wide" : "");
  source->AddBoolean("realboxIsTall", redesigned_modules_enabled);
  DefineChromeRefreshRealboxIcons();
  source->AddString(
      "searchboxDefaultIcon",
      base::FeatureList::IsEnabled(ntp_features::kRealboxUseGoogleGIcon)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
          ? kGoogleGIconResourceName
#else
          ? kSearchIconResourceName
#endif
          : kSearchIconResourceName);

  source->AddBoolean("searchboxVoiceSearch", enable_voice_search);
  source->AddBoolean("searchboxLensSearch", enable_lens_search);
  source->AddString("searchboxLensVariations", GetBase64UrlVariations(profile));
  source->AddBoolean(
      "searchboxLensDirectUpload",
      base::FeatureList::IsEnabled(ntp_features::kNtpLensDirectUpload));
  source->AddBoolean(
      "searchboxCr23Theming",
      base::FeatureList::IsEnabled(ntp_features::kRealboxCr23Theming));
  source->AddBoolean("searchboxCr23SteadyStateShadow",
                     ntp_features::kNtpRealboxCr23SteadyStateShadow.Get());
}

// static
std::string SearchboxHandler::AutocompleteMatchVectorIconToResourceName(
    const gfx::VectorIcon& icon) {
  if (icon.name == omnibox::kAnswerCurrencyIcon.name ||
      icon.name == omnibox::kAnswerCurrencyChromeRefreshIcon.name) {
    return kAnswerCurrencyIconResourceName;
  } else if (icon.name == omnibox::kAnswerDefaultIcon.name) {
    return kAnswerDefaultIconResourceName;
  } else if (icon.name == omnibox::kAnswerDictionaryIcon.name ||
             icon.name == omnibox::kAnswerDictionaryChromeRefreshIcon.name) {
    return kAnswerDictionaryIconResourceName;
  } else if (icon.name == omnibox::kAnswerFinanceIcon.name ||
             icon.name == omnibox::kAnswerFinanceChromeRefreshIcon.name) {
    return kAnswerFinanceIconResourceName;
  } else if (icon.name == omnibox::kAnswerSunriseIcon.name ||
             icon.name == omnibox::kAnswerSunriseChromeRefreshIcon.name) {
    return kAnswerSunriseIconResourceName;
  } else if (icon.name == omnibox::kAnswerTranslationIcon.name ||
             icon.name == omnibox::kAnswerTranslationChromeRefreshIcon.name) {
    return kAnswerTranslationIconResourceName;
  } else if (icon.name == omnibox::kAnswerWhenIsIcon.name ||
             icon.name == omnibox::kAnswerWhenIsChromeRefreshIcon.name) {
    return kAnswerWhenIsIconResourceName;
  } else if (icon.name == omnibox::kBookmarkIcon.name ||
             icon.name == omnibox::kBookmarkChromeRefreshIcon.name) {
    return kBookmarkIconResourceName;
  } else if (icon.name == omnibox::kCalculatorIcon.name ||
             icon.name == omnibox::kCalculatorChromeRefreshIcon.name) {
    return kCalculatorIconResourceName;
  } else if (icon.name == omnibox::kClockIcon.name ||
             icon.name == omnibox::kClockChromeRefreshIcon.name) {
    return kClockIconResourceName;
  } else if (icon.name == omnibox::kDriveDocsIcon.name) {
    return kDriveDocsIconResourceName;
  } else if (icon.name == omnibox::kDriveFolderIcon.name) {
    return kDriveFolderIconResourceName;
  } else if (icon.name == omnibox::kDriveFormsIcon.name) {
    return kDriveFormIconResourceName;
  } else if (icon.name == omnibox::kDriveImageIcon.name) {
    return kDriveImageIconResourceName;
  } else if (icon.name == omnibox::kDriveLogoIcon.name) {
    return kDriveLogoIconResourceName;
  } else if (icon.name == omnibox::kDrivePdfIcon.name) {
    return kDrivePdfIconResourceName;
  } else if (icon.name == omnibox::kDriveSheetsIcon.name) {
    return kDriveSheetsIconResourceName;
  } else if (icon.name == omnibox::kDriveSlidesIcon.name) {
    return kDriveSlidesIconResourceName;
  } else if (icon.name == omnibox::kDriveVideoIcon.name) {
    return kDriveVideoIconResourceName;
  } else if (icon.name == omnibox::kExtensionAppIcon.name) {
    return kExtensionAppIconResourceName;
  } else if (icon.name == vector_icons::kHistoryIcon.name ||
             icon.name == vector_icons::kHistoryChromeRefreshIcon.name) {
    return kHistoryIconResourceName;
  } else if (icon.name == omnibox::kJourneysIcon.name ||
             icon.name == omnibox::kJourneysChromeRefreshIcon.name) {
    return kJourneysIconResourceName;
  } else if (icon.name == omnibox::kPageIcon.name ||
             icon.name == omnibox::kPageChromeRefreshIcon.name) {
    return kPageIconResourceName;
  } else if (icon.name == omnibox::kProductIcon.name ||
             icon.name == omnibox::kProductChromeRefreshIcon.name) {
    return kChromeProductIconResourceName;
  } else if (icon.name == vector_icons::kSearchIcon.name ||
             icon.name == vector_icons::kSearchChromeRefreshIcon.name) {
    return kSearchIconResourceName;
  } else if (icon.name == omnibox::kTrendingUpIcon.name ||
             icon.name == omnibox::kTrendingUpChromeRefreshIcon.name) {
    return kTrendingUpIconResourceName;
  } else if (icon.is_empty()) {
    return "";  // An empty resource name is effectively a blank icon.
  } else {
    return ActionVectorIconToResourceName(icon);
  }
}

// static
std::string SearchboxHandler::ActionVectorIconToResourceName(
    const gfx::VectorIcon& icon) {
  if (icon.name == omnibox::kSwitchIcon.name ||
      icon.name == omnibox::kSwitchCr2023Icon.name) {
    return kTabIconResourceName;
  }
  if (icon.name == omnibox::kDinoIcon.name ||
      icon.name == omnibox::kDinoCr2023Icon.name) {
    return kDinoIconResourceName;
  }
  if (icon.name == omnibox::kDriveFormsIcon.name) {
    return kDriveFormIconResourceName;
  }
  if (icon.name == omnibox::kDriveDocsIcon.name) {
    return kDriveDocsIconResourceName;
  }
  if (icon.name == omnibox::kDriveSheetsIcon.name) {
    return kDriveSheetsIconResourceName;
  }
  if (icon.name == omnibox::kDriveSlidesIcon.name) {
    return kDriveSlidesIconResourceName;
  }
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (icon.name == vector_icons::kGoogleCalendarIcon.name) {
    return kGoogleCalendarIconResourceName;
  }
  if (icon.name == vector_icons::kGoogleKeepNoteIcon.name) {
    return kGoogleKeepNoteIconResourceName;
  }
  if (icon.name == vector_icons::kGoogleSitesIcon.name) {
    return kGoogleSitesIconResourceName;
  }
  if (icon.name == vector_icons::kGoogleSuperGIcon.name ||
      icon.name == vector_icons::kGoogleGLogoMonochromeIcon.name) {
    return kGoogleGIconResourceName;
  }
#endif
  if (icon.name == omnibox::kIncognitoIcon.name ||
      icon.name == omnibox::kIncognitoCr2023Icon.name) {
    return kIncognitoIconResourceName;
  }
  if (icon.name == omnibox::kJourneysIcon.name ||
      icon.name == omnibox::kJourneysChromeRefreshIcon.name) {
    return kJourneysIconResourceName;
  }
  if (icon.name == omnibox::kPedalIcon.name ||
      icon.name == omnibox::kProductChromeRefreshIcon.name) {
    return kPedalsIconResourceName;
  }
#if BUILDFLAG(IS_MAC)
  if (icon.name == omnibox::kShareMacIcon.name ||
      icon.name == omnibox::kShareMacChromeRefreshIcon.name) {
    return kMacShareIconResourceName;
  }
#elif BUILDFLAG(IS_WIN)
  if (icon.name == omnibox::kShareWinIcon.name ||
      icon.name == omnibox::kShareWinChromeRefreshIcon.name) {
    return kWinShareIconResourceName;
  }
#elif BUILDFLAG(IS_LINUX)
  if (icon.name == omnibox::kShareIcon.name ||
      icon.name == omnibox::kShareLinuxChromeRefreshIcon.name) {
    return kLinuxShareIconResourceName;
  }
#else
  if (icon.name == omnibox::kShareIcon.name ||
      icon.name == omnibox::kShareChromeRefreshIcon.name) {
    return kShareIconResourceName;
  }
#endif
  if (icon.name == omnibox::kSparkIcon.name) {
    return kSparkIconResourceName;
  }
  if (icon.name == omnibox::kStarActiveIcon.name ||
      icon.name == omnibox::kStarActiveChromeRefreshIcon.name) {
    return kStarActiveIconResourceName;
  }
  NOTREACHED_IN_MIGRATION()
      << "Every vector icon returned by OmniboxAction::GetVectorIcon "
         "must have an equivalent SVG resource for the NTP Realbox. "
         "icon.name: '"
      << icon.name << "'";
  return "";
}

SearchboxHandler::SearchboxHandler(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
    Profile* profile,
    content::WebContents* web_contents,
    MetricsReporter* metrics_reporter)
    : profile_(profile),
      web_contents_(web_contents),
      metrics_reporter_(metrics_reporter),
      page_set_(false),
      page_handler_(this, std::move(pending_page_handler)) {}

SearchboxHandler::~SearchboxHandler() {
  // Avoids dangling pointer warning when `controller_` is not owned.
  controller_ = nullptr;
}

void SearchboxHandler::OnResultChanged(AutocompleteController* controller,
                                       bool default_match_changed) {
  if (metrics_reporter_ && !metrics_reporter_->HasLocalMark("ResultChanged")) {
    metrics_reporter_->Mark("ResultChanged");
  }
  page_->AutocompleteResultChanged(CreateAutocompleteResult(
      autocomplete_controller()->input().text(),
      autocomplete_controller()->result(),
      BookmarkModelFactory::GetForBrowserContext(profile_),
      profile_->GetPrefs(),
      omnibox_controller()->client()->GetTemplateURLService()));

  // The owned OmniboxController does not observe the AutocompleteController.
  // Notify the prerender here to start preloading if the results are ready.
  // TODO(crbug.com/40062053): Make the owned OmniboxController observe the
  //  AutocompleteController and move this logic to the RealboxOmniboxClient.
  if (owned_controller_) {
    if (autocomplete_controller()->done()) {
      if (SearchPrefetchService* search_prefetch_service =
              SearchPrefetchServiceFactory::GetForProfile(profile_)) {
        search_prefetch_service->OnResultChanged(
            web_contents_, autocomplete_controller()->result());
      }
    }
  }
}

OmniboxController* SearchboxHandler::omnibox_controller() const {
  return controller_;
}

AutocompleteController* SearchboxHandler::autocomplete_controller() const {
  return omnibox_controller()->autocomplete_controller();
}
