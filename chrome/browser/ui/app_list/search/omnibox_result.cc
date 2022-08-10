// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/omnibox_result.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "base/callback_forward.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/chromeos/launcher_search/search_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/search/common/icon_constants.h"
#include "chrome/browser/ui/app_list/search/omnibox_util.h"
#include "chrome/browser/ui/app_list/search/search_tags_util.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/crosapi/mojom/launcher_search.mojom.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/search_engines/util.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/common/image_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"
#include "url/url_canon.h"

using CrosApiSearchResult = crosapi::mojom::SearchResult;

namespace app_list {
namespace {

// Priority numbers for deduplication. Higher numbers indicate higher priority.
constexpr int kRichEntityPriority = 2;
constexpr int kHistoryPriority = 1;
constexpr int kDefaultPriority = 0;

// crosapi OmniboxType to vector icon, used for app list.
const gfx::VectorIcon& TypeToVectorIcon(CrosApiSearchResult::OmniboxType type) {
  switch (type) {
    case CrosApiSearchResult::OmniboxType::kDomain:
      return ash::kOmniboxGenericIcon;
    case CrosApiSearchResult::OmniboxType::kSearch:
      return ash::kSearchIcon;
    case CrosApiSearchResult::OmniboxType::kHistory:
      return ash::kHistoryIcon;
    default:
      NOTREACHED();
      return ash::kOmniboxGenericIcon;
  }
}

// Returns tags for the given text, with match tags manually included for
// compatibility with the classic launcher.
ash::SearchResultTags TagsForTextWithMatchTags(
    const std::u16string& query,
    const std::u16string& text,
    CrosApiSearchResult::TextType type) {
  ash::SearchResultTags tags = CalculateTags(query, text);
  for (const ash::SearchResultTag tag : TagsForText(text, type))
    tags.push_back(tag);
  return tags;
}

}  // namespace

OmniboxResult::OmniboxResult(Profile* profile,
                             AppListControllerDelegate* list_controller,
                             base::RepeatingClosure remove_closure,
                             crosapi::mojom::SearchResultPtr search_result,
                             const std::u16string& query,
                             bool is_zero_suggestion)
    : consumer_receiver_(this, std::move(search_result->receiver)),
      profile_(profile),
      list_controller_(list_controller),
      search_result_(std::move(search_result)),
      remove_closure_(std::move(remove_closure)),
      query_(query),
      is_zero_suggestion_(is_zero_suggestion),
      contents_(search_result_->contents.value_or(u"")),
      description_(search_result_->description.value_or(u"")) {
  SetDisplayType(DisplayType::kList);
  SetResultType(ResultType::kOmnibox);
  SetMetricsType(GetSearchResultType());

  set_id(search_result_->stripped_destination_url->spec());

  // Omnibox results are categorized as Search and Assistant if they are search
  // suggestions, and Web otherwise.
  SetCategory(search_result_->omnibox_type ==
                      CrosApiSearchResult::OmniboxType::kSearch
                  ? Category::kSearchAndAssistant
                  : Category::kWeb);

  // Derive relevance from omnibox relevance and normalize it to [0, 1].
  set_relevance(search_result_->relevance / kMaxOmniboxScore);

  if (IsRichEntity()) {
    dedup_priority_ = kRichEntityPriority;
  } else if (search_result_->omnibox_type ==
             CrosApiSearchResult::OmniboxType::kHistory) {
    dedup_priority_ = kHistoryPriority;
  } else {
    dedup_priority_ = kDefaultPriority;
  }

  SetIsOmniboxSearch(
      crosapi::OptionalBoolIsTrue(search_result_->is_omnibox_search));
  SetSkipUpdateAnimation(search_result_->metrics_type ==
                         CrosApiSearchResult::MetricsType::kSearchWhatYouTyped);

  UpdateIcon();
  UpdateTitleAndDetails();

  if (is_zero_suggestion_) {
    DCHECK(!ash::features::IsProductivityLauncherEnabled());
    InitializeButtonActions({ash::SearchResultActionType::kRemove,
                             ash::SearchResultActionType::kAppend});
  } else if (crosapi::OptionalBoolIsTrue(search_result_->is_omnibox_search) &&
             ash::features::IsProductivityLauncherEnabled()) {
    InitializeButtonActions({ash::SearchResultActionType::kRemove});
  }

  if (auto* dark_light_mode_controller = ash::DarkLightModeController::Get())
    dark_light_mode_controller->AddObserver(this);
}

OmniboxResult::~OmniboxResult() {
  if (auto* dark_light_mode_controller = ash::DarkLightModeController::Get())
    dark_light_mode_controller->RemoveObserver(this);
}

void OmniboxResult::Open(int event_flags) {
  list_controller_->OpenURL(profile_, *search_result_->destination_url,
                            crosapi::PageTransitionToUiPageTransition(
                                search_result_->page_transition),
                            ui::DispositionFromEventFlags(event_flags));
}

void OmniboxResult::InvokeAction(ash::SearchResultActionType action) {
  switch (action) {
    case ash::SearchResultActionType::kRemove:
      remove_closure_.Run();
      break;
    case ash::SearchResultActionType::kAppend:
    case ash::SearchResultActionType::kSearchResultActionTypeMax:
      NOTREACHED();
  }
}

ash::SearchResultType OmniboxResult::GetSearchResultType() const {
  switch (search_result_->metrics_type) {
    case CrosApiSearchResult::MetricsType::kWhatYouTyped:
      return ash::OMNIBOX_URL_WHAT_YOU_TYPED;
    case CrosApiSearchResult::MetricsType::kRecentlyVisitedWebsite:
      return ash::OMNIBOX_RECENTLY_VISITED_WEBSITE;
    case CrosApiSearchResult::MetricsType::kHistoryTitle:
      return ash::OMNIBOX_RECENT_DOC_IN_DRIVE;
    case CrosApiSearchResult::MetricsType::kSearchWhatYouTyped:
      return ash::OMNIBOX_WEB_QUERY;
    case CrosApiSearchResult::MetricsType::kSearchHistory:
      return ash::OMNIBOX_SEARCH_HISTORY;
    case CrosApiSearchResult::MetricsType::kSearchSuggest:
      return ash::OMNIBOX_SEARCH_SUGGEST;
    case CrosApiSearchResult::MetricsType::kSearchSuggestPersonalized:
      return ash::OMNIBOX_SUGGEST_PERSONALIZED;
    case CrosApiSearchResult::MetricsType::kBookmark:
      return ash::OMNIBOX_BOOKMARK;
    // SEARCH_SUGGEST_ENTITY corresponds with rich entity results.
    case CrosApiSearchResult::MetricsType::kSearchSuggestEntity:
      return ash::OMNIBOX_SEARCH_SUGGEST_ENTITY;
    case CrosApiSearchResult::MetricsType::kNavSuggest:
      return ash::OMNIBOX_NAVSUGGEST;

    default:
      return ash::SEARCH_RESULT_TYPE_BOUNDARY;
  }
}

void OmniboxResult::OnColorModeChanged(bool dark_mode_enabled) {
  if (uses_generic_icon_)
    SetGenericIcon();
}

void OmniboxResult::OnFaviconReceived(const gfx::ImageSkia& icon) {
  // By contract, this is never called with an empty |icon|.
  DCHECK(!icon.isNull());
  search_result_->favicon = icon;
  SetIcon(IconInfo(icon, kFaviconDimension));
}

void OmniboxResult::UpdateIcon() {
  if (IsRichEntity()) {
    FetchRichEntityImage(*search_result_->image_url);
    return;
  }

  // Use a favicon if eligible. In the event that a favicon becomes available
  // asynchronously, it will be sent to us over Mojo and we will update our
  // icon.
  if (!search_result_->favicon.isNull()) {
    SetIcon(IconInfo(search_result_->favicon, kFaviconDimension));
    return;
  }

  SetGenericIcon();
}

void OmniboxResult::SetGenericIcon() {
  uses_generic_icon_ = true;
  // If this is neither a rich entity nor eligible for a favicon, use either
  // the generic bookmark or another generic icon as appropriate.
  if (search_result_->omnibox_type ==
      CrosApiSearchResult::OmniboxType::kBookmark) {
    SetIcon(IconInfo(
        gfx::CreateVectorIcon(omnibox::kBookmarkIcon, kSystemIconDimension,
                              GetGenericIconColor()),
        kSystemIconDimension));
  } else {
    SetIcon(IconInfo(
        gfx::CreateVectorIcon(TypeToVectorIcon(search_result_->omnibox_type),
                              kSystemIconDimension, GetGenericIconColor()),
        kSystemIconDimension));
  }
}

void OmniboxResult::UpdateTitleAndDetails() {
  if (!IsUrlResultWithDescription()) {
    SetTitle(contents_);
    SetTitleTags(TagsForTextWithMatchTags(query_, contents_,
                                          search_result_->contents_type));

    if (IsRichEntity()) {
      SetDetails(description_);
      SetDetailsTags(TagsForTextWithMatchTags(
          query_, description_, search_result_->description_type));

      // Append the search engine to the accessible name only.
      const std::u16string accessible_name =
          details().empty() ? title()
                            : base::StrCat({title(), u", ", details()});
      SetAccessibleName(l10n_util::GetStringFUTF16(
          IDS_APP_LIST_QUERY_SEARCH_ACCESSIBILITY_NAME, accessible_name,
          GetDefaultSearchEngineName(
              TemplateURLServiceFactory::GetForProfile(profile_))));
    } else if (crosapi::OptionalBoolIsTrue(search_result_->is_omnibox_search)) {
      // For non-rich-entity results, put the search engine into the details
      // field. Tags are not used since this does not change with the query.
      SetDetails(l10n_util::GetStringFUTF16(
          IDS_AUTOCOMPLETE_SEARCH_DESCRIPTION,
          GetDefaultSearchEngineName(
              TemplateURLServiceFactory::GetForProfile(profile_))));
    }
  } else {
    // For url result with non-empty description, swap title and details. Thus,
    // the url description is presented as title, and url itself is presented as
    // details.
    SetTitle(description_);
    SetTitleTags(TagsForTextWithMatchTags(query_, description_,
                                          search_result_->description_type));

    SetDetails(contents_);
    SetDetailsTags(TagsForTextWithMatchTags(query_, contents_,
                                            search_result_->contents_type));
  }
}

bool OmniboxResult::IsUrlResultWithDescription() const {
  return !(crosapi::OptionalBoolIsTrue(search_result_->is_omnibox_search) ||
           description_.empty());
}

bool OmniboxResult::IsRichEntity() const {
  return search_result_->image_url.value_or(GURL()).is_valid();
}

void OmniboxResult::FetchRichEntityImage(const GURL& url) {
  if (!bitmap_fetcher_) {
    bitmap_fetcher_ =
        std::make_unique<BitmapFetcher>(url, this, kOmniboxTrafficAnnotation);
  }
  bitmap_fetcher_->Init(net::ReferrerPolicy::NEVER_CLEAR,
                        network::mojom::CredentialsMode::kOmit);
  bitmap_fetcher_->Start(profile_->GetURLLoaderFactory().get());
}

void OmniboxResult::OnFetchComplete(const GURL& url, const SkBitmap* bitmap) {
  if (!bitmap)
    return;

  IconInfo icon_info(gfx::ImageSkia::CreateFrom1xBitmap(*bitmap),
                     GetImageIconDimension(), IconShape::kRoundedRectangle);
  SetIcon(icon_info);
}

void OmniboxResult::InitializeButtonActions(
    const std::vector<ash::SearchResultActionType>& button_actions) {
  Actions actions;
  for (ash::SearchResultActionType button_action : button_actions) {
    std::u16string button_tooltip;
    bool visible_on_hover = false;

    switch (button_action) {
      case ash::SearchResultActionType::kRemove:
        button_tooltip = l10n_util::GetStringFUTF16(
            IDS_APP_LIST_REMOVE_SUGGESTION_ACCESSIBILITY_NAME, title());
        visible_on_hover = true;  // visible upon hovering
        break;
      case ash::SearchResultActionType::kAppend:
        button_tooltip = l10n_util::GetStringFUTF16(
            IDS_APP_LIST_APPEND_SUGGESTION_ACCESSIBILITY_NAME, title());
        visible_on_hover = false;  // always visible
        break;
      default:
        NOTREACHED();
    }
    Action search_action(button_action, button_tooltip, visible_on_hover);
    actions.emplace_back(search_action);
  }

  SetActions(actions);
}

}  // namespace app_list
