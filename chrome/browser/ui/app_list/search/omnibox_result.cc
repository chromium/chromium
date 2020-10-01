// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/omnibox_result.h"

#include <stddef.h>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/search_engines/util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"
#include "url/url_canon.h"

using bookmarks::BookmarkModel;

namespace app_list {

namespace {

constexpr SkColor kListIconColor = gfx::kGoogleGrey700;

int ACMatchStyleToTagStyle(int styles) {
  int tag_styles = 0;
  if (styles & ACMatchClassification::URL)
    tag_styles |= ash::SearchResultTag::URL;
  if (styles & ACMatchClassification::MATCH)
    tag_styles |= ash::SearchResultTag::MATCH;
  if (styles & ACMatchClassification::DIM)
    tag_styles |= ash::SearchResultTag::DIM;

  return tag_styles;
}

// Translates ACMatchClassifications into ChromeSearchResult tags.
void ACMatchClassificationsToTags(const base::string16& text,
                                  const ACMatchClassifications& text_classes,
                                  ChromeSearchResult::Tags* tags) {
  int tag_styles = ash::SearchResultTag::NONE;
  size_t tag_start = 0;

  for (size_t i = 0; i < text_classes.size(); ++i) {
    const ACMatchClassification& text_class = text_classes[i];

    // Closes current tag.
    if (tag_styles != ash::SearchResultTag::NONE) {
      tags->push_back(
          ash::SearchResultTag(tag_styles, tag_start, text_class.offset));
      tag_styles = ash::SearchResultTag::NONE;
    }

    if (text_class.style == ACMatchClassification::NONE)
      continue;

    tag_start = text_class.offset;
    tag_styles = ACMatchStyleToTagStyle(text_class.style);
  }

  if (tag_styles != ash::SearchResultTag::NONE) {
    tags->push_back(ash::SearchResultTag(tag_styles, tag_start, text.length()));
  }
}

// AutocompleteMatchType::Type to vector icon, used for app list.
const gfx::VectorIcon& TypeToVectorIcon(AutocompleteMatchType::Type type) {
  switch (type) {
    case AutocompleteMatchType::URL_WHAT_YOU_TYPED:
    case AutocompleteMatchType::HISTORY_URL:
    case AutocompleteMatchType::HISTORY_TITLE:
    case AutocompleteMatchType::HISTORY_BODY:
    case AutocompleteMatchType::HISTORY_KEYWORD:
    case AutocompleteMatchType::NAVSUGGEST:
    case AutocompleteMatchType::BOOKMARK_TITLE:
    case AutocompleteMatchType::NAVSUGGEST_PERSONALIZED:
    case AutocompleteMatchType::CLIPBOARD_URL:
    case AutocompleteMatchType::PHYSICAL_WEB_DEPRECATED:
    case AutocompleteMatchType::PHYSICAL_WEB_OVERFLOW_DEPRECATED:
    case AutocompleteMatchType::TAB_SEARCH_DEPRECATED:
    case AutocompleteMatchType::DOCUMENT_SUGGESTION:
    case AutocompleteMatchType::PEDAL:
      return ash::kDomainIcon;

    case AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED:
    case AutocompleteMatchType::SEARCH_SUGGEST:
    case AutocompleteMatchType::SEARCH_SUGGEST_ENTITY:
    case AutocompleteMatchType::SEARCH_SUGGEST_TAIL:
    case AutocompleteMatchType::SEARCH_SUGGEST_PROFILE:
    case AutocompleteMatchType::SEARCH_OTHER_ENGINE:
    case AutocompleteMatchType::CONTACT_DEPRECATED:
    case AutocompleteMatchType::VOICE_SUGGEST:
    case AutocompleteMatchType::CLIPBOARD_TEXT:
    case AutocompleteMatchType::CLIPBOARD_IMAGE:
      return ash::kSearchIcon;

    case AutocompleteMatchType::SEARCH_HISTORY:
    case AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED:
      return ash::kHistoryIcon;

    case AutocompleteMatchType::CALCULATOR:
      return ash::kEqualIcon;

    case AutocompleteMatchType::EXTENSION_APP_DEPRECATED:
    case AutocompleteMatchType::TILE_SUGGESTION:
    case AutocompleteMatchType::TILE_NAVSUGGEST:
    case AutocompleteMatchType::NUM_TYPES:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return ash::kDomainIcon;
}

base::string16 ImageLineToString16(const SuggestionAnswer::ImageLine& line) {
  std::vector<base::string16> text;
  for (const auto& text_field : line.text_fields()) {
    text.push_back(text_field.text());
  }
  // TODO(crbug.com/1130372): Use placeholders or a l10n-friendly way to
  // construct this string instead of concatenation.
  return base::JoinString(text, base::ASCIIToUTF16(" "));
}

}  // namespace

OmniboxResult::OmniboxResult(Profile* profile,
                             AppListControllerDelegate* list_controller,
                             AutocompleteController* autocomplete_controller,
                             const AutocompleteMatch& match,
                             bool is_zero_suggestion)
    : profile_(profile),
      list_controller_(list_controller),
      autocomplete_controller_(autocomplete_controller),
      match_(match),
      is_zero_suggestion_(is_zero_suggestion) {
  if (match_.search_terms_args && autocomplete_controller_) {
    match_.search_terms_args->from_app_list = true;
    autocomplete_controller_->UpdateMatchDestinationURL(
        *match_.search_terms_args, &match_);
  }
  set_id(match_.stripped_destination_url.spec());
  SetResultType(ash::AppListSearchResultType::kOmnibox);
  set_result_subtype(static_cast<int>(match_.type));
  SetMetricsType(GetSearchResultType());

  if (app_list_features::IsOmniboxRichEntitiesEnabled()) {
    SetIsAnswer(match_.answer.has_value());
    if (is_answer()) {
      // The answer subtype overrides the match subtype.
      set_result_subtype(static_cast<int>(match_.answer->type()));
    }
  }

  // Derive relevance from omnibox relevance and normalize it to [0, 1].
  // The magic number 1500 is the highest score of an omnibox result.
  // See comments in autocomplete_provider.h.
  set_relevance(match_.relevance / 1500.0);

  if (AutocompleteMatch::IsSearchType(match_.type))
    SetIsOmniboxSearch(true);
  UpdateIcon();
  UpdateTitleAndDetails();

  if (is_zero_suggestion_)
    SetZeroSuggestionActions();
}

OmniboxResult::~OmniboxResult() = default;

void OmniboxResult::Open(int event_flags) {
  RecordOmniboxResultHistogram();
  list_controller_->OpenURL(profile_, match_.destination_url, match_.transition,
                            ui::DispositionFromEventFlags(event_flags));
}

void OmniboxResult::Remove() {
  // TODO(jennyz): add RecordHistogram.
  autocomplete_controller_->DeleteMatch(match_);
}

void OmniboxResult::InvokeAction(int action_index, int event_flags) {
  DCHECK(is_zero_suggestion_);
  switch (ash::GetOmniBoxZeroStateAction(action_index)) {
    case ash::OmniBoxZeroStateAction::kRemoveSuggestion:
      Remove();
      break;
    default:
      NOTREACHED();
  }
}

ash::SearchResultType OmniboxResult::GetSearchResultType() const {
  switch (match_.type) {
    case AutocompleteMatchType::URL_WHAT_YOU_TYPED:
      return ash::OMNIBOX_URL_WHAT_YOU_TYPED;
    case AutocompleteMatchType::HISTORY_URL: {
      BookmarkModel* bookmark_model =
          BookmarkModelFactory::GetForBrowserContext(profile_);
      if (bookmark_model &&
          bookmark_model->IsBookmarked(match_.destination_url)) {
        return ash::OMNIBOX_BOOKMARK;
      }
      return ash::OMNIBOX_RECENTLY_VISITED_WEBSITE;
    }
    case AutocompleteMatchType::HISTORY_TITLE:
      return ash::OMNIBOX_RECENT_DOC_IN_DRIVE;
    case AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED:
      return ash::OMNIBOX_WEB_QUERY;
    case AutocompleteMatchType::SEARCH_HISTORY:
      return ash::OMNIBOX_SEARCH_HISTORY;
    case AutocompleteMatchType::SEARCH_SUGGEST:
      return ash::OMNIBOX_SEARCH_SUGGEST;
    case AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED:
      return ash::OMNIBOX_SUGGEST_PERSONALIZED;
    case AutocompleteMatchType::BOOKMARK_TITLE:
      return ash::OMNIBOX_BOOKMARK;

    case AutocompleteMatchType::HISTORY_KEYWORD:
    case AutocompleteMatchType::NAVSUGGEST:
    case AutocompleteMatchType::SEARCH_SUGGEST_ENTITY:
    case AutocompleteMatchType::SEARCH_SUGGEST_TAIL:
    case AutocompleteMatchType::SEARCH_SUGGEST_PROFILE:
    case AutocompleteMatchType::SEARCH_OTHER_ENGINE:
    case AutocompleteMatchType::CONTACT_DEPRECATED:
    case AutocompleteMatchType::NAVSUGGEST_PERSONALIZED:
    case AutocompleteMatchType::CALCULATOR:
    case AutocompleteMatchType::CLIPBOARD_URL:
    case AutocompleteMatchType::VOICE_SUGGEST:
    case AutocompleteMatchType::PHYSICAL_WEB_DEPRECATED:
    case AutocompleteMatchType::PHYSICAL_WEB_OVERFLOW_DEPRECATED:
    case AutocompleteMatchType::EXTENSION_APP_DEPRECATED:
    case AutocompleteMatchType::TAB_SEARCH_DEPRECATED:
    case AutocompleteMatchType::DOCUMENT_SUGGESTION:
    case AutocompleteMatchType::PEDAL:
    case AutocompleteMatchType::CLIPBOARD_TEXT:
    case AutocompleteMatchType::CLIPBOARD_IMAGE:
    case AutocompleteMatchType::HISTORY_BODY:
    case AutocompleteMatchType::TILE_SUGGESTION:
    case AutocompleteMatchType::TILE_NAVSUGGEST:
    case AutocompleteMatchType::NUM_TYPES:
      // TODO(crbug.com/1028447): Add a NOTREACHED here once we are confident we
      // know all possible types for this result.
      return ash::SEARCH_RESULT_TYPE_BOUNDARY;
  }
}

GURL OmniboxResult::DestinationURL() const {
  return match_.destination_url;
}

void OmniboxResult::UpdateIcon() {
  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile_);
  bool is_bookmarked =
      bookmark_model && bookmark_model->IsBookmarked(match_.destination_url);

  const gfx::VectorIcon& icon =
      is_bookmarked ? omnibox::kBookmarkIcon : TypeToVectorIcon(match_.type);
  SetIcon(gfx::CreateVectorIcon(
      icon, ash::AppListConfig::instance().search_list_icon_dimension(),
      kListIconColor));
}

void OmniboxResult::UpdateTitleAndDetails() {
  // For url result with non-empty description, swap title and details. Thus,
  // the url description is presented as title, and url itself is presented as
  // details.
  const bool use_directly = !IsUrlResultWithDescription();
  ChromeSearchResult::Tags title_tags;
  if (ShouldDisplayAsAnswer()) {
    const auto* additional_text = match_.answer->first_line().additional_text();
    const bool has_additional_text =
        additional_text && !additional_text->text().empty();
    // TODO(crbug.com/1130372): Use placeholders or a l10n-friendly way to
    // construct this string instead of concatenation.
    SetTitle(has_additional_text
                 ? base::StrCat({match_.contents, base::ASCIIToUTF16(" "),
                                 additional_text->text()})
                 : match_.contents);
    ACMatchClassificationsToTags(match_.contents, match_.contents_class,
                                 &title_tags);
  } else if (use_directly) {
    SetTitle(match_.contents);
    ACMatchClassificationsToTags(match_.contents, match_.contents_class,
                                 &title_tags);
  } else {
    SetTitle(match_.description);
    ACMatchClassificationsToTags(match_.description, match_.description_class,
                                 &title_tags);
  }
  SetTitleTags(title_tags);

  ChromeSearchResult::Tags details_tags;
  if (ShouldDisplayAsAnswer()) {
    // Answer results will contain the answer in the second line.
    SetDetails(ImageLineToString16(match_.answer->second_line()));
  } else if (use_directly) {
    if (AutocompleteMatch::IsSearchType(match_.type)) {
      SetAccessibleName(l10n_util::GetStringFUTF16(
          IDS_APP_LIST_QUERY_SEARCH_ACCESSIBILITY_NAME, title(),
          GetDefaultSearchEngineName(
              TemplateURLServiceFactory::GetForProfile(profile_))));
    }
    SetDetails(match_.description);
    ACMatchClassificationsToTags(match_.description, match_.description_class,
                                 &details_tags);
  } else {
    SetDetails(match_.contents);
    ACMatchClassificationsToTags(match_.contents, match_.contents_class,
                                 &details_tags);
  }
  SetDetailsTags(details_tags);
}

bool OmniboxResult::IsUrlResultWithDescription() const {
  return !AutocompleteMatch::IsSearchType(match_.type) &&
         !match_.description.empty();
}

void OmniboxResult::SetZeroSuggestionActions() {
  Actions zero_suggestion_actions;

  constexpr int kMaxButtons = ash::OmniBoxZeroStateAction::kZeroStateActionMax;
  for (int i = 0; i < kMaxButtons; ++i) {
    ash::OmniBoxZeroStateAction button_action =
        ash::GetOmniBoxZeroStateAction(i);
    gfx::ImageSkia button_image;
    base::string16 button_tooltip;
    bool visible_on_hover = false;
    const int kImageButtonIconSize =
        ash::AppListConfig::instance().search_list_badge_icon_dimension();

    switch (button_action) {
      case ash::OmniBoxZeroStateAction::kRemoveSuggestion:
        button_image = gfx::CreateVectorIcon(
            ash::kSearchResultRemoveIcon, kImageButtonIconSize, kListIconColor);
        button_tooltip = l10n_util::GetStringFUTF16(
            IDS_APP_LIST_REMOVE_SUGGESTION_ACCESSIBILITY_NAME, title());
        visible_on_hover = true;  // visible upon hovering
        break;
      case ash::OmniBoxZeroStateAction::kAppendSuggestion:
        button_image = gfx::CreateVectorIcon(
            ash::kSearchResultAppendIcon, kImageButtonIconSize, kListIconColor);
        button_tooltip = l10n_util::GetStringFUTF16(
            IDS_APP_LIST_APPEND_SUGGESTION_ACCESSIBILITY_NAME, title());
        visible_on_hover = false;  // always visible
        break;
      default:
        NOTREACHED();
    }
    Action search_action(button_image, button_tooltip, visible_on_hover);
    zero_suggestion_actions.emplace_back(search_action);
  }

  SetActions(zero_suggestion_actions);
}

void OmniboxResult::RecordOmniboxResultHistogram() {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppListSearchOmniboxResultOpenType",
                            is_zero_suggestion_
                                ? OmniboxResultType::kZeroStateSuggestion
                                : OmniboxResultType::kQuerySuggestion);
}

bool OmniboxResult::ShouldDisplayAsAnswer() {
  return app_list_features::IsOmniboxRichEntitiesEnabled() && is_answer();
}

}  // namespace app_list
