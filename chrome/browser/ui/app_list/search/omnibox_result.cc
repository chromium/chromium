// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/omnibox_result.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/omnibox_util.h"
#include "chrome/browser/ui/app_list/search/search_tags_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/favicon_cache.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/search_engines/util.h"
#include "extensions/common/image_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
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
    autocomplete_controller_->SetMatchDestinationURL(&match_);
  }
  SetDisplayType(DisplayType::kList);
  SetResultType(ResultType::kOmnibox);
  SetMetricsType(GetSearchResultType());
  set_id(match_.stripped_destination_url.spec());

  // Omnibox results are categorized as Search and Assistant if they are search
  // suggestions, and Web otherwise.
  switch (match_.type) {
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
      SetCategory(Category::kSearchAndAssistant);
      break;
    default:
      SetCategory(Category::kWeb);
      break;
  }

  // Derive relevance from omnibox relevance and normalize it to [0, 1].
  set_relevance(match_.relevance / kMaxOmniboxScore);

  const bool is_omnibox_search = AutocompleteMatch::IsSearchType(match_.type);
  SetIsOmniboxSearch(is_omnibox_search);

  UpdateIcon();
  UpdateTitleAndDetails();

  if (is_zero_suggestion_) {
    InitializeButtonActions({ash::SearchResultActionType::kRemove,
                             ash::SearchResultActionType::kAppend});
  } else if (is_omnibox_search &&
             ash::features::IsProductivityLauncherEnabled()) {
    InitializeButtonActions({ash::SearchResultActionType::kRemove});
  }
}

OmniboxResult::~OmniboxResult() = default;

void OmniboxResult::Open(int event_flags) {
  list_controller_->OpenURL(profile_, match_.destination_url, match_.transition,
                            ui::DispositionFromEventFlags(event_flags));
}

void OmniboxResult::Remove() {
  // TODO(jennyz): add RecordHistogram.
  autocomplete_controller_->DeleteMatch(match_);
}

void OmniboxResult::InvokeAction(ash::SearchResultActionType action) {
  switch (action) {
    case ash::SearchResultActionType::kRemove:
      Remove();
      break;
    case ash::SearchResultActionType::kAppend:
    case ash::SearchResultActionType::kSearchResultActionTypeMax:
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
    // SEARCH_SUGGEST_ENTITY corresponds with rich entity results.
    case AutocompleteMatchType::SEARCH_SUGGEST_ENTITY:
      return ash::OMNIBOX_SEARCH_SUGGEST_ENTITY;
    case AutocompleteMatchType::NAVSUGGEST:
      return ash::OMNIBOX_NAVSUGGEST;
    case AutocompleteMatchType::CALCULATOR:
      return ash::OMNIBOX_CALCULATOR;

    case AutocompleteMatchType::HISTORY_KEYWORD:
    case AutocompleteMatchType::SEARCH_SUGGEST_TAIL:
    case AutocompleteMatchType::SEARCH_SUGGEST_PROFILE:
    case AutocompleteMatchType::SEARCH_OTHER_ENGINE:
    case AutocompleteMatchType::CONTACT_DEPRECATED:
    case AutocompleteMatchType::NAVSUGGEST_PERSONALIZED:
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

void OmniboxResult::UpdateIcon() {
  if (IsRichEntity()) {
    FetchRichEntityImage(match_.image_url);
    return;
  }

  // Use a favicon if eligible. If the result should have a favicon but
  // there isn't one in the cache, fall through to using a generic icon
  // instead.
  if (favicon_cache_ && MatchTypeToIconType(match_.type) == IconType::kDomain) {
    const auto icon = favicon_cache_->GetFaviconForPageUrl(
        match_.destination_url, base::BindOnce(&OmniboxResult::OnFaviconFetched,
                                               weak_factory_.GetWeakPtr()));
    if (!icon.IsEmpty()) {
      SetIcon(
          IconInfo(icon.AsImageSkia(), ash::SharedAppListConfig::instance()
                                           .search_list_favicon_dimension()));
      return;
    }
  }

  // If this is neither a rich entity nor eligible for a favicon, use either
  // the generic bookmark or another generic icon as appropriate.
  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile_);
  if (bookmark_model && bookmark_model->IsBookmarked(match_.destination_url)) {
    SetIcon(IconInfo(
        gfx::CreateVectorIcon(
            omnibox::kBookmarkIcon,
            ash::SharedAppListConfig::instance().search_list_icon_dimension(),
            kListIconColor),
        ash::SharedAppListConfig::instance().search_list_icon_dimension()));
  } else {
    SetIcon(IconInfo(
        gfx::CreateVectorIcon(
            TypeToVectorIcon(match_.type),
            ash::SharedAppListConfig::instance().search_list_icon_dimension(),
            kListIconColor),
        ash::SharedAppListConfig::instance().search_list_icon_dimension()));
  }
}

void OmniboxResult::UpdateTitleAndDetails() {
  if (!IsUrlResultWithDescription()) {
    SetTitle(match_.contents);
    ChromeSearchResult::Tags title_tags;
    ACMatchClassificationsToTags(match_.contents, match_.contents_class,
                                 &title_tags);
    SetTitleTags(title_tags);

    if (IsRichEntity()) {
      // Only set the details text for rich entities. This prevents default
      // descriptions such as "Google Search" from being added.
      SetDetails(match_.description);
      ChromeSearchResult::Tags details_tags;
      ACMatchClassificationsToTags(match_.description, match_.description_class,
                                   &details_tags);
      SetDetailsTags(details_tags);
    }

    if (AutocompleteMatch::IsSearchType(match_.type)) {
      std::u16string accessible_name =
          details().empty() ? title()
                            : base::StrCat({title(), u", ", details()});
      SetAccessibleName(l10n_util::GetStringFUTF16(
          IDS_APP_LIST_QUERY_SEARCH_ACCESSIBILITY_NAME, accessible_name,
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

bool OmniboxResult::IsRichEntity() const {
  return !match_.image_url.is_empty();
}

void OmniboxResult::FetchRichEntityImage(const GURL& url) {
  if (!bitmap_fetcher_) {
    bitmap_fetcher_ =
        std::make_unique<BitmapFetcher>(url, this, kOmniboxTrafficAnnotation);
  }
  bitmap_fetcher_->Init(/*referrer=*/std::string(),
                        net::ReferrerPolicy::NEVER_CLEAR,
                        network::mojom::CredentialsMode::kOmit);
  bitmap_fetcher_->Start(profile_->GetURLLoaderFactory().get());
}

void OmniboxResult::OnFetchComplete(const GURL& url, const SkBitmap* bitmap) {
  if (!bitmap)
    return;

  IconInfo icon_info(gfx::ImageSkia::CreateFrom1xBitmap(*bitmap));
  icon_info.dimension =
      ash::SharedAppListConfig::instance().search_list_image_icon_dimension();
  icon_info.shape = IconShape::kRoundedRectangle;
  SetIcon(icon_info);
}

void OmniboxResult::OnFaviconFetched(const gfx::Image& icon) {
  // By contract, this is never called with an empty |icon|.
  DCHECK(!icon.IsEmpty());
  SetIcon(IconInfo(
      icon.AsImageSkia(),
      ash::SharedAppListConfig::instance().search_list_favicon_dimension()));
}

void OmniboxResult::InitializeButtonActions(
    const std::vector<ash::SearchResultActionType>& button_actions) {
  Actions actions;
  for (ash::SearchResultActionType button_action : button_actions) {
    gfx::ImageSkia button_image;
    std::u16string button_tooltip;
    bool visible_on_hover = false;
    const int kImageButtonIconSize =
        ash::SharedAppListConfig::instance().search_list_icon_dimension();

    switch (button_action) {
      case ash::SearchResultActionType::kRemove:
        button_image = gfx::CreateVectorIcon(
            ash::kSearchResultRemoveIcon, kImageButtonIconSize, kListIconColor);
        button_tooltip = l10n_util::GetStringFUTF16(
            IDS_APP_LIST_REMOVE_SUGGESTION_ACCESSIBILITY_NAME, title());
        visible_on_hover = true;  // visible upon hovering
        break;
      case ash::SearchResultActionType::kAppend:
        button_image = gfx::CreateVectorIcon(
            ash::kSearchResultAppendIcon, kImageButtonIconSize, kListIconColor);
        button_tooltip = l10n_util::GetStringFUTF16(
            IDS_APP_LIST_APPEND_SUGGESTION_ACCESSIBILITY_NAME, title());
        visible_on_hover = false;  // always visible
        break;
      default:
        NOTREACHED();
    }
    Action search_action(button_action, button_image, button_tooltip,
                         visible_on_hover);
    actions.emplace_back(search_action);
  }

  SetActions(actions);
}

}  // namespace app_list
