// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/omnibox_result.h"

#include <stddef.h>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/search_tags_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/favicon_cache.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/search_engines/util.h"
#include "extensions/common/image_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"
#include "url/url_canon.h"

using bookmarks::BookmarkModel;

namespace app_list {

namespace {

constexpr SkColor kListIconColor = gfx::kGoogleGrey700;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("cros_launcher_omnibox", R"(
        semantics {
          sender: "Chrome OS Launcher"
          description:
            "Chrome OS provides search suggestions when a user types a query "
            "into the launcher. This request downloads an image icon for a "
            "suggested result in order to provide more information."
          trigger:
            "Change of results for the query typed by the user into the "
            "launcher."
          data:
            "URL of the image to be downloaded. This URL corresponds to "
            "search suggestions for the user's query."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Search autocomplete and suggestions can be disabled in Chrome OS "
            "settings. Image icons cannot be disabled separately to this."
          policy_exception_justification:
            "No content is uploaded or saved, this request downloads a "
            "publicly available image."
        })");

// Types of generic icon to show with a result.
enum class IconType {
  kDomain,
  kSearch,
  kHistory,
  kCalculator,
};

const IconType MatchTypeToIconType(AutocompleteMatchType::Type type) {
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
    case AutocompleteMatchType::PEDAL_DEPRECATED:
      return IconType::kDomain;

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
      return IconType::kSearch;

    case AutocompleteMatchType::SEARCH_HISTORY:
    case AutocompleteMatchType::SEARCH_SUGGEST_PERSONALIZED:
      return IconType::kHistory;

    case AutocompleteMatchType::CALCULATOR:
      return IconType::kCalculator;

    case AutocompleteMatchType::EXTENSION_APP_DEPRECATED:
    case AutocompleteMatchType::TILE_SUGGESTION:
    case AutocompleteMatchType::TILE_NAVSUGGEST:
    case AutocompleteMatchType::NUM_TYPES:
      NOTREACHED();
      return IconType::kDomain;
  }
}

// AutocompleteMatchType::Type to vector icon, used for app list.
const gfx::VectorIcon& TypeToVectorIcon(AutocompleteMatchType::Type type) {
  switch (MatchTypeToIconType(type)) {
    case IconType::kDomain:
      return ash::kOmniboxGenericIcon;
    case IconType::kSearch:
      return ash::kSearchIcon;
    case IconType::kHistory:
      return ash::kHistoryIcon;
    case IconType::kCalculator:
      return ash::kEqualIcon;
  }
}

// Converts AutocompleteMatchType::Type to an answer vector icon.
const gfx::VectorIcon& TypeToAnswerIcon(int type) {
  switch (static_cast<SuggestionAnswer::AnswerType>(type)) {
    case SuggestionAnswer::ANSWER_TYPE_CURRENCY:
      return omnibox::kAnswerCurrencyIcon;
    case SuggestionAnswer::ANSWER_TYPE_DICTIONARY:
      return omnibox::kAnswerDictionaryIcon;
    case SuggestionAnswer::ANSWER_TYPE_FINANCE:
      return omnibox::kAnswerFinanceIcon;
    case SuggestionAnswer::ANSWER_TYPE_SUNRISE:
      return omnibox::kAnswerSunriseIcon;
    case SuggestionAnswer::ANSWER_TYPE_TRANSLATION:
      return omnibox::kAnswerTranslationIcon;
    case SuggestionAnswer::ANSWER_TYPE_WHEN_IS:
      return omnibox::kAnswerWhenIsIcon;
    default:
      return omnibox::kAnswerDefaultIcon;
  }
}

gfx::ImageSkia CreateAnswerIcon(const gfx::VectorIcon& vector_icon) {
  const auto& icon = gfx::CreateVectorIcon(vector_icon, SK_ColorWHITE);
  const int dimension =
      ash::SharedAppListConfig::instance().search_list_answer_icon_dimension();
  return gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
      dimension / 2, gfx::kGoogleBlue600, icon);
}

absl::optional<std::u16string> GetAdditionalText(
    const SuggestionAnswer::ImageLine& line) {
  if (line.additional_text()) {
    const auto additional_text = line.additional_text()->text();
    if (!additional_text.empty())
      return additional_text;
  }
  return absl::nullopt;
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
  // TODO(crbug.com/1130372): Use placeholders or a l10n-friendly way to
  // construct this string instead of concatenation. This currently only happens
  // for stock ticker symbols.
  return base::JoinString(text, u" ");
}

}  // namespace

OmniboxResult::OmniboxResult(Profile* profile,
                             AppListControllerDelegate* list_controller,
                             AutocompleteController* autocomplete_controller,
                             FaviconCache* favicon_cache,
                             const AutocompleteMatch& match,
                             bool is_zero_suggestion)
    : profile_(profile),
      list_controller_(list_controller),
      autocomplete_controller_(autocomplete_controller),
      favicon_cache_(favicon_cache),
      match_(match),
      is_zero_suggestion_(is_zero_suggestion) {
  if (match_.search_terms_args && autocomplete_controller_) {
    match_.search_terms_args->request_source = TemplateURLRef::CROS_APP_LIST;
    autocomplete_controller_->UpdateMatchDestinationURL(
        *match_.search_terms_args, &match_);
  }
  set_id(match_.stripped_destination_url.spec());
  SetDisplayType(DisplayType::kList);
  SetResultType(ResultType::kOmnibox);
  SetMetricsType(GetSearchResultType());

  if (app_list_features::IsOmniboxRichEntitiesEnabled()) {
    if (match_.answer.has_value()) {
      SetOmniboxType(OmniboxType::kAnswer);
    } else if (match_.type == AutocompleteMatchType::CALCULATOR) {
      SetOmniboxType(OmniboxType::kCalculatorAnswer);
    } else if (!match_.image_url.is_empty()) {
      SetOmniboxType(OmniboxType::kRichImage);
    }

    // The stripped destination URL is no longer a unique identifier, so append
    // it to the omnibox type.
    const std::string id = base::JoinString(
        {base::NumberToString(static_cast<int>(omnibox_type())),
         match_.stripped_destination_url.spec()},
        "-");
    set_id(id);
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

void OmniboxResult::InvokeAction(int action_index) {
  DCHECK(is_zero_suggestion_);
  switch (ash::GetOmniBoxZeroStateAction(action_index)) {
    case ash::OmniBoxZeroStateAction::kRemoveSuggestion:
      Remove();
      break;
    default:
      NOTREACHED();
  }
}

void OmniboxResult::OnFetchComplete(const GURL& url, const SkBitmap* bitmap) {
  if (bitmap)
    SetIcon(gfx::ImageSkia::CreateFrom1xBitmap(*bitmap));
}

ash::SearchResultType OmniboxResult::GetSearchResultType() const {
  // Rich entity types take precedence.
  if (omnibox_type() == OmniboxType::kAnswer ||
      omnibox_type() == OmniboxType::kCalculatorAnswer) {
    return ash::OMNIBOX_RICH_ENTITY_ANSWER;
  }
  if (omnibox_type() == OmniboxType::kRichImage)
    return ash::OMNIBOX_RICH_ENTITY_IMAGE_ENTITY;

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
    case AutocompleteMatchType::SEARCH_SUGGEST_ENTITY:
      return ash::OMNIBOX_SEARCH_SUGGEST_ENTITY;
    case AutocompleteMatchType::NAVSUGGEST:
      return ash::OMNIBOX_NAVSUGGEST;

    case AutocompleteMatchType::HISTORY_KEYWORD:
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
    case AutocompleteMatchType::PEDAL_DEPRECATED:
    case AutocompleteMatchType::CLIPBOARD_TEXT:
    case AutocompleteMatchType::CLIPBOARD_IMAGE:
    case AutocompleteMatchType::HISTORY_BODY:
    case AutocompleteMatchType::TILE_SUGGESTION:
    case AutocompleteMatchType::TILE_NAVSUGGEST:
    case AutocompleteMatchType::NUM_TYPES:
      return ash::SEARCH_RESULT_TYPE_BOUNDARY;
  }
}

GURL OmniboxResult::DestinationURL() const {
  return match_.destination_url;
}

void OmniboxResult::UpdateIcon() {
  switch (omnibox_type()) {
    case OmniboxType::kCalculatorAnswer:
      SetIcon(CreateAnswerIcon(omnibox::kCalculatorIcon));
      return;
    case OmniboxType::kAnswer:
      if (match_.answer->type() == SuggestionAnswer::ANSWER_TYPE_WEATHER &&
          !match_.answer->image_url().is_empty()) {
        // Weather icons are downloaded. Check this first so that the local
        // default answer icon can be used as a fallback if the URL is missing.
        FetchRichEntityImage(match_.answer->image_url());
      } else {
        SetIcon(CreateAnswerIcon(TypeToAnswerIcon(match_.answer->type())));
      }
      return;
    case OmniboxType::kRichImage:
      FetchRichEntityImage(match_.image_url);
      return;
    default:
      // Use a favicon if eligible. If the result should have a favicon but
      // there isn't one in the cache, fall through to using a generic icon
      // instead.
      if (favicon_cache_ &&
          MatchTypeToIconType(match_.type) == IconType::kDomain) {
        const auto icon = favicon_cache_->GetFaviconForPageUrl(
            match_.destination_url,
            base::BindOnce(&OmniboxResult::OnFaviconFetched,
                           weak_factory_.GetWeakPtr()));
        if (!icon.IsEmpty()) {
          SetOmniboxType(OmniboxType::kFavicon);
          SetIcon(icon.AsImageSkia());
          return;
        }
      }

      // If this is neither a rich entity nor eligible for a favicon, use either
      // the generic bookmark or another generic icon as appropriate.
      BookmarkModel* bookmark_model =
          BookmarkModelFactory::GetForBrowserContext(profile_);
      if (bookmark_model &&
          bookmark_model->IsBookmarked(match_.destination_url)) {
        SetIcon(gfx::CreateVectorIcon(
            omnibox::kBookmarkIcon,
            ash::SharedAppListConfig::instance().search_list_icon_dimension(),
            kListIconColor));
      } else {
        SetIcon(gfx::CreateVectorIcon(
            TypeToVectorIcon(match_.type),
            ash::SharedAppListConfig::instance().search_list_icon_dimension(),
            kListIconColor));
      }
  }
}

void OmniboxResult::UpdateTitleAndDetails() {
  if (app_list_features::IsOmniboxRichEntitiesEnabled() &&
      match_.answer.has_value()) {
    const auto& additional_text =
        GetAdditionalText(match_.answer->first_line());
    // TODO(crbug.com/1130372): Use placeholders or a l10n-friendly way to
    // construct this string instead of concatenation. This currently only
    // happens for stock ticker symbols.
    SetTitle(
        additional_text
            ? base::JoinString({match_.contents, additional_text.value()}, u" ")
            : match_.contents);
    SetDetails(ImageLineToString16(match_.answer->second_line()));
  } else if (!IsUrlResultWithDescription()) {
    SetTitle(match_.contents);
    ChromeSearchResult::Tags title_tags;
    ACMatchClassificationsToTags(match_.contents, match_.contents_class,
                                 &title_tags);
    SetTitleTags(title_tags);

    if (!app_list_features::IsOmniboxRichEntitiesEnabled() ||
        match_.type == AutocompleteMatchType::CALCULATOR ||
        match_.type == AutocompleteMatchType::SEARCH_SUGGEST_ENTITY) {
      // Only set the details text for rich entity or calculator results. This
      // prevents default descriptions such as "Google Search" from being added.
      SetDetails(match_.description);
      ChromeSearchResult::Tags details_tags;
      ACMatchClassificationsToTags(match_.description, match_.description_class,
                                   &details_tags);
      SetDetailsTags(details_tags);
    }

    if (AutocompleteMatch::IsSearchType(match_.type)) {
      SetAccessibleName(l10n_util::GetStringFUTF16(
          IDS_APP_LIST_QUERY_SEARCH_ACCESSIBILITY_NAME, title(),
          GetDefaultSearchEngineName(
              TemplateURLServiceFactory::GetForProfile(profile_))));
    }
  } else {
    // For url result with non-empty description, swap title and details. Thus,
    // the url description is presented as title, and url itself is presented as
    // details.
    SetTitle(match_.description);
    ChromeSearchResult::Tags title_tags;
    ACMatchClassificationsToTags(match_.description, match_.description_class,
                                 &title_tags);
    SetTitleTags(title_tags);

    SetDetails(match_.contents);
    ChromeSearchResult::Tags details_tags;
    ACMatchClassificationsToTags(match_.contents, match_.contents_class,
                                 &details_tags);
    SetDetailsTags(details_tags);
  }
}

bool OmniboxResult::IsUrlResultWithDescription() const {
  return !AutocompleteMatch::IsSearchType(match_.type) &&
         !match_.description.empty();
}

void OmniboxResult::FetchRichEntityImage(const GURL& url) {
  if (!bitmap_fetcher_) {
    bitmap_fetcher_ =
        std::make_unique<BitmapFetcher>(url, this, kTrafficAnnotation);
  }
  bitmap_fetcher_->Init(/*referrer=*/std::string(),
                        net::ReferrerPolicy::NEVER_CLEAR,
                        network::mojom::CredentialsMode::kOmit);
  bitmap_fetcher_->Start(profile_->GetURLLoaderFactory().get());
}

void OmniboxResult::OnFaviconFetched(const gfx::Image& icon) {
  // By contract, this is never called with an empty |icon|.
  DCHECK(!icon.IsEmpty());
  SetOmniboxType(OmniboxType::kFavicon);
  SetIcon(icon.AsImageSkia());
}

void OmniboxResult::SetZeroSuggestionActions() {
  Actions zero_suggestion_actions;

  constexpr int kMaxButtons = ash::OmniBoxZeroStateAction::kZeroStateActionMax;
  for (int i = 0; i < kMaxButtons; ++i) {
    ash::OmniBoxZeroStateAction button_action =
        ash::GetOmniBoxZeroStateAction(i);
    gfx::ImageSkia button_image;
    std::u16string button_tooltip;
    bool visible_on_hover = false;
    const int kImageButtonIconSize =
        ash::SharedAppListConfig::instance().search_list_badge_icon_dimension();

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

}  // namespace app_list
