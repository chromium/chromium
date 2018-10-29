// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/omnibox_result.h"

#include <stddef.h>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/search_util.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/vector_icons.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"
#include "url/url_canon.h"

using bookmarks::BookmarkModel;

namespace app_list {

namespace {

// #000 at 87% opacity.
constexpr SkColor kListIconColor = SkColorSetARGB(0xDE, 0x00, 0x00, 0x00);

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
    case AutocompleteMatchType::CLIPBOARD:
    case AutocompleteMatchType::PHYSICAL_WEB_DEPRECATED:
    case AutocompleteMatchType::PHYSICAL_WEB_OVERFLOW_DEPRECATED:
    case AutocompleteMatchType::TAB_SEARCH_DEPRECATED:
    case AutocompleteMatchType::DOCUMENT_SUGGESTION:
    case AutocompleteMatchType::PEDAL:
      return kIcDomainIcon;

    case AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED:
    case AutocompleteMatchType::SEARCH_SUGGEST:
    case AutocompleteMatchType::SEARCH_SUGGEST_ENTITY:
    case AutocompleteMatchType::SEARCH_SUGGEST_TAIL:
    case AutocompleteMatchType::SEARCH_SUGGEST_PROFILE:
    case AutocompleteMatchType::SEARCH_OTHER_ENGINE:
    case AutocompleteMatchType::CONTACT_DEPRECATED:
    case AutocompleteMatchType::VOICE_SUGGEST:
      return kIcSearchIcon;

    case AutocompleteMatchType::SEARCH_HISTORY:
    case AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED:
      return kIcHistoryIcon;

    case AutocompleteMatchType::CALCULATOR:
      return kIcEqualIcon;

    case AutocompleteMatchType::EXTENSION_APP_DEPRECATED:
    case AutocompleteMatchType::NUM_TYPES:
      NOTREACHED();
      break;
  }
  NOTREACHED();
  return kIcDomainIcon;
}

}  // namespace

OmniboxResult::OmniboxResult(Profile* profile,
                             AppListControllerDelegate* list_controller,
                             AutocompleteController* autocomplete_controller,
                             const AutocompleteMatch& match)
    : profile_(profile),
      list_controller_(list_controller),
      autocomplete_controller_(autocomplete_controller),
      match_(match) {
  if (match_.search_terms_args && autocomplete_controller_) {
    match_.search_terms_args->from_app_list = true;
    autocomplete_controller_->UpdateMatchDestinationURL(
        *match_.search_terms_args, &match_);
  }
  set_id(match_.destination_url.spec());
  set_comparable_id(match_.stripped_destination_url.spec());
  SetResultType(ash::SearchResultType::kOmnibox);

  // Derive relevance from omnibox relevance and normalize it to [0, 1].
  // The magic number 1500 is the highest score of an omnibox result.
  // See comments in autocomplete_provider.h.
  set_relevance(match_.relevance / 1500.0);

  if (AutocompleteMatch::IsSearchType(match_.type))
    SetIsOmniboxSearch(true);

  UpdateIcon();
  UpdateTitleAndDetails();
}

OmniboxResult::~OmniboxResult() = default;

void OmniboxResult::Open(int event_flags) {
  RecordHistogram(OMNIBOX_SEARCH_RESULT);
  list_controller_->OpenURL(profile_, match_.destination_url, match_.transition,
                            ui::DispositionFromEventFlags(event_flags));
}

void OmniboxResult::UpdateIcon() {
  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile_);
  bool is_bookmarked =
      bookmark_model && bookmark_model->IsBookmarked(match_.destination_url);

  const gfx::VectorIcon& icon =
      is_bookmarked ? kIcBookmarkIcon : TypeToVectorIcon(match_.type);
  SetIcon(gfx::CreateVectorIcon(
      icon, AppListConfig::instance().search_list_icon_dimension(),
      kListIconColor));
}

void OmniboxResult::UpdateTitleAndDetails() {
  // For url result with non-empty description, swap title and details. Thus,
  // the url description is presented as title, and url itself is presented as
  // details.
  const bool use_directly = !IsUrlResultWithDescription();
  ChromeSearchResult::Tags title_tags;
  if (use_directly) {
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
  if (use_directly) {
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

}  // namespace app_list
