// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"

#include <stddef.h>

#include <algorithm>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/back_to_opener/back_to_opener_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/common/url_constants.h"
#include "components/favicon_base/favicon_types.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/accelerators/menu_label_accelerator_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/text_elider.h"
#include "ui/menus/simple_menu_model.h"

using base::UserMetricsAction;
using content::NavigationController;
using content::NavigationEntry;
using content::WebContents;

const size_t BackForwardMenuModel::kMaxHistoryItems = 12;
const size_t BackForwardMenuModel::kMaxChapterStops = 5;
static const int kMaxBackForwardMenuWidth = 700;

BackForwardMenuModel::BackForwardMenuModel(Browser* browser,
                                           ModelType model_type)
    : browser_(browser), model_type_(model_type) {}

BackForwardMenuModel::~BackForwardMenuModel() = default;

base::WeakPtr<ui::MenuModel> BackForwardMenuModel::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

size_t BackForwardMenuModel::GetItemCount() const {
  size_t total = 0;

  // Sum up all sections in order
  for (MenuSection section :
       {MenuSection::kHistory, MenuSection::kSeparator,
        MenuSection::kChapterStops, MenuSection::kBackToOpener,
        MenuSection::kShowFullHistory}) {
    total += GetSectionItemCount(section);
  }

  return total;
}

ui::MenuModel::ItemType BackForwardMenuModel::GetTypeAt(size_t index) const {
  return IsSeparator(index) ? TYPE_SEPARATOR : TYPE_COMMAND;
}

ui::MenuSeparatorType BackForwardMenuModel::GetSeparatorTypeAt(
    size_t index) const {
  return ui::NORMAL_SEPARATOR;
}

int BackForwardMenuModel::GetCommandIdAt(size_t index) const {
  return static_cast<int>(index);
}

std::u16string BackForwardMenuModel::GetLabelAt(size_t index) const {
  std::optional<MenuSection> section = GetSectionForIndex(index);
  if (!section.has_value()) {
    return std::u16string();
  }

  switch (section.value()) {
    case MenuSection::kHistory:
    case MenuSection::kChapterStops: {
      // Return the entry title, escaping any '&' characters and eliding it if
      // it's super long.
      NavigationEntry* entry = GetNavigationEntry(index);
      std::u16string menu_text(entry->GetTitleForDisplay());
      menu_text = ui::EscapeMenuLabelAmpersands(menu_text);
      menu_text = gfx::ElideText(menu_text, gfx::FontList(),
                                 kMaxBackForwardMenuWidth, gfx::ELIDE_TAIL);
      return menu_text;
    }
    case MenuSection::kBackToOpener: {
      return back_to_opener::BackToOpenerController::GetFormattedOpenerTitle(
          GetWebContents());
    }
    case MenuSection::kSeparator:
      return std::u16string();
    case MenuSection::kShowFullHistory:
      return l10n_util::GetStringUTF16(IDS_HISTORY_SHOWFULLHISTORY_LINK);
  }
}

bool BackForwardMenuModel::IsItemDynamicAt(size_t index) const {
  // This object is only used for a single showing of a menu.
  return false;
}

bool BackForwardMenuModel::GetAcceleratorAt(
    size_t index,
    ui::Accelerator* accelerator) const {
  return false;
}

bool BackForwardMenuModel::IsItemCheckedAt(size_t index) const {
  return false;
}

int BackForwardMenuModel::GetGroupIdAt(size_t index) const {
  return false;
}

ui::ImageModel BackForwardMenuModel::GetIconAt(size_t index) const {
  std::optional<MenuSection> section = GetSectionForIndex(index);
  if (!section.has_value()) {
    return ui::ImageModel();
  }

  switch (section.value()) {
    case MenuSection::kHistory:
    case MenuSection::kChapterStops: {
      NavigationEntry* entry = GetNavigationEntry(index);
      content::FaviconStatus fav_icon = entry->GetFavicon();
      if (!fav_icon.valid && menu_model_delegate()) {
        // FetchFavicon is not const because it caches the result, but GetIconAt
        // is const because it is not be apparent to outside observers that an
        // internal change is taking place. Compared to spreading const in
        // unintuitive places (e.g. making menu_model_delegate() const but
        // returning a non-const while sprinkling virtual on member variables),
        // this const_cast is the lesser evil.
        const_cast<BackForwardMenuModel*>(this)->FetchFavicon(entry);
      }

      // Only apply theming to certain chrome:// favicons.
      if (favicon::ShouldThemifyFaviconForEntry(entry)) {
        const ui::ColorProvider* const cp =
            &GetWebContents()->GetColorProvider();
        gfx::ImageSkia themed_favicon = favicon::ThemeFavicon(
            fav_icon.image.AsImageSkia(), cp->GetColor(ui::kColorMenuIcon),
            cp->GetColor(ui::kColorMenuItemBackgroundHighlighted),
            cp->GetColor(ui::kColorMenuBackground));
        return ui::ImageModel::FromImageSkia(themed_favicon);
      }

      return ui::ImageModel::FromImage(fav_icon.image);
    }
    case MenuSection::kBackToOpener:
      return back_to_opener::BackToOpenerController::GetOpenerMenuIcon(
          GetWebContents());
    case MenuSection::kSeparator:
      return ui::ImageModel();
    case MenuSection::kShowFullHistory:
      return ui::ImageModel::FromVectorIcon(
          kHistoryIcon, ui::kColorMenuIcon,
          ui::SimpleMenuModel::kDefaultIconSize);
  }
}

ui::ButtonMenuItemModel* BackForwardMenuModel::GetButtonMenuItemAt(
    size_t index) const {
  return nullptr;
}

bool BackForwardMenuModel::IsEnabledAt(size_t index) const {
  std::optional<MenuSection> section = GetSectionForIndex(index);
  return section.has_value() && section.value() != MenuSection::kSeparator;
}

ui::MenuModel* BackForwardMenuModel::GetSubmenuModelAt(size_t index) const {
  return nullptr;
}

void BackForwardMenuModel::ActivatedAt(size_t index) {
  ActivatedAt(index, 0);
}

void BackForwardMenuModel::ActivatedAt(size_t index, int event_flags) {
  DCHECK(!IsSeparator(index));

  std::optional<MenuSection> section = GetSectionForIndex(index);
  if (!section.has_value()) {
    return;
  }

  switch (section.value()) {
    case MenuSection::kHistory: {
      base::RecordComputedAction(BuildActionName("HistoryClick", index));
      break;
    }
    case MenuSection::kChapterStops: {
      // Calculate chapter index: use section start to get accurate offset.
      std::optional<size_t> chapter_start =
          GetStartingIndexOfSection(MenuSection::kChapterStops);
      CHECK(chapter_start.has_value());
      size_t chapter_index = index - chapter_start.value();
      base::RecordComputedAction(
          BuildActionName("ChapterClick", chapter_index));
      break;
    }
    case MenuSection::kBackToOpener: {
      base::RecordComputedAction(
          BuildActionName("BackToOpenerClick", std::nullopt));
      back_to_opener::BackToOpenerController::GoBackToOpener(GetWebContents());
      return;
    }
    case MenuSection::kSeparator:
      return;
    case MenuSection::kShowFullHistory:
      base::RecordComputedAction(
          BuildActionName("ShowFullHistory", std::nullopt));
      ShowSingletonTabOverwritingNTP(browser_,
                                     GURL(chrome::kChromeUIHistoryURL));
      return;
  }

  CHECK(menu_model_open_timestamp_.has_value());
  base::TimeDelta time =
      base::TimeTicks::Now() - menu_model_open_timestamp_.value();
  base::UmaHistogramLongTimes(
      "Navigation.BackForward.TimeFromOpenBackNavigationMenuToActivateItem",
      time);

  std::optional<size_t> controller_index = MenuIndexToNavEntryIndex(index);
  DCHECK(controller_index.has_value());

  WindowOpenDisposition disposition =
      ui::DispositionFromEventFlags(event_flags);
  chrome::NavigateToIndexWithDisposition(browser_, controller_index.value(),
                                         disposition);
}

void BackForwardMenuModel::MenuWillShow() {
  base::RecordComputedAction(BuildActionName("Popup", std::nullopt));
  requested_favicons_.clear();
  cancelable_task_tracker_.TryCancelAll();
  menu_model_open_timestamp_ = base::TimeTicks::Now();
  // Observe the web contents for navigation changes which could
  // happen while the menu is open.
  content::WebContentsObserver::Observe(GetWebContents());

  // Close the IPH popup if the user opens the menu.
  BrowserUserEducationInterface::From(browser_)->NotifyFeaturePromoFeatureUsed(
      feature_engagement::kIPHBackNavigationMenuFeature,
      FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
}

void BackForwardMenuModel::MenuWillClose() {
  content::WebContentsObserver::Observe(nullptr);
  CHECK(menu_model_open_timestamp_.has_value());
  base::TimeDelta time =
      base::TimeTicks::Now() - menu_model_open_timestamp_.value();
  base::UmaHistogramLongTimes(
      "Navigation.BackForward.TimeFromOpenBackNavigationMenuToCloseMenu", time);
}

void BackForwardMenuModel::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  if (menu_model_delegate()) {
    menu_model_delegate()->OnMenuStructureChanged();
  }
}

void BackForwardMenuModel::NavigationEntriesDeleted() {
  if (menu_model_delegate()) {
    menu_model_delegate()->OnMenuStructureChanged();
  }
}

std::optional<BackForwardMenuModel::MenuSection>
BackForwardMenuModel::GetSectionForIndex(size_t index) const {
  if (index >= GetItemCount()) {
    return std::nullopt;
  }

  size_t history_items = GetHistoryItemCount();

  // History section: indices 0 to history_items-1
  if (index < history_items) {
    return MenuSection::kHistory;
  }

  // Chapter stops section: appears when menu is full (kMaxHistoryItems) and
  // there are domain transitions. They're separated from history items by a
  // separator.
  if (HasSection(MenuSection::kChapterStops)) {
    // Separator is right after history items (before chapter stops)
    if (index == history_items) {
      return MenuSection::kSeparator;
    }
    // Chapter stops start after history items and separator
    std::optional<size_t> chapter_stops_start =
        GetStartingIndexOfSection(MenuSection::kChapterStops);
    CHECK(chapter_stops_start.has_value());
    size_t chapter_stops_count =
        GetSectionItemCount(MenuSection::kChapterStops);
    if (index >= chapter_stops_start.value() &&
        index < chapter_stops_start.value() + chapter_stops_count) {
      return MenuSection::kChapterStops;
    }
  }

  // Back-to-opener section: comes after history and chapter stops (if they
  // exist), and before the separator and "Show Full History" sections.
  bool has_back_to_opener =
      back_to_opener::BackToOpenerController::HasValidOpener(GetWebContents());
  if (has_back_to_opener) {
    std::optional<size_t> opener_index =
        GetStartingIndexOfSection(MenuSection::kBackToOpener);
    if (opener_index.has_value() && index == opener_index.value()) {
      return MenuSection::kBackToOpener;
    }
  }

  // "Show Full History" and separator sections:
  // "Show Full History" is always the last item if it exists.
  if (HasSection(MenuSection::kShowFullHistory)) {
    size_t item_count = GetItemCount();
    if (index == item_count - 1) {
      return MenuSection::kShowFullHistory;
    }
    // Separator is right before "Show Full History"
    if (index == item_count - 2) {
      return MenuSection::kSeparator;
    }
  }

  return std::nullopt;
}

bool BackForwardMenuModel::HasSection(MenuSection section) const {
  return GetSectionItemCount(section) > 0;
}

std::optional<size_t> BackForwardMenuModel::GetStartingIndexOfSection(
    MenuSection section) const {
  // Separators can appear at multiple positions in the menu (before chapter
  // stops and/or before "Show Full History"), so there's no single "starting
  // index" for the separator section. This function should not be called with
  // kSeparator. Instead, separators are identified by their specific index
  // positions in GetSectionForIndex().
  CHECK(section != MenuSection::kSeparator);

  if (!HasSection(section)) {
    return std::nullopt;
  }

  switch (section) {
    case MenuSection::kHistory:
      return 0;  // History starts at 0
    case MenuSection::kChapterStops: {
      // Chapter stops start after history items and the separator before them.
      // Since HasSection already verified chapter stops exist, there's always
      // exactly one separator before them.
      return GetHistoryItemCount() + 1;
    }
    case MenuSection::kBackToOpener: {
      // Back-to-opener comes before the separator and "Show Full History"
      // sections.
      size_t item_count = GetItemCount();
      if (HasSection(MenuSection::kShowFullHistory)) {
        // "Show Full History" is at item_count - 1, separator at item_count -
        // 2, so back-to-opener is at item_count - 3
        return item_count - 3;
      }
      // If "Show Full History" doesn't exist, back-to-opener is the last item
      return item_count - 1;
    }
    case MenuSection::kSeparator:
      NOTREACHED();
    case MenuSection::kShowFullHistory:
      // "Show Full History" is always the last item in the menu.
      return GetItemCount() - 1;
  }
}

size_t BackForwardMenuModel::GetSectionItemCount(MenuSection section) const {
  switch (section) {
    case MenuSection::kHistory:
      return GetHistoryItemCount();
    case MenuSection::kChapterStops: {
      size_t history_items = GetHistoryItemCount();
      return GetChapterStopCount(history_items);
    }
    case MenuSection::kBackToOpener:
      return back_to_opener::BackToOpenerController::HasValidOpener(
                 GetWebContents())
                 ? 1u
                 : 0u;
    case MenuSection::kSeparator:
      return GetSeparatorCount();
    case MenuSection::kShowFullHistory:
      // "Show Full History" should only be visible if the menu has items and
      // we're not in incognito mode. And when we have opener.
      return (ShouldShowFullHistoryBeVisible() &&
              (GetHistoryItemCount() > 0 ||
               back_to_opener::BackToOpenerController::HasValidOpener(
                   GetWebContents())))
                 ? 1u
                 : 0u;
  }
}

bool BackForwardMenuModel::IsSeparator(size_t index) const {
  std::optional<MenuSection> section = GetSectionForIndex(index);
  return section.has_value() && section.value() == MenuSection::kSeparator;
}

void BackForwardMenuModel::FetchFavicon(NavigationEntry* entry) {
  // If the favicon has already been requested for this menu, don't do
  // anything.
  if (requested_favicons_.contains(entry->GetUniqueID())) {
    return;
  }

  requested_favicons_.insert(entry->GetUniqueID());
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(browser_->profile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!favicon_service) {
    return;
  }

  favicon_service->GetFaviconImageForPageURL(
      entry->GetURL(),
      base::BindOnce(&BackForwardMenuModel::OnFavIconDataAvailable,
                     base::Unretained(this), entry->GetUniqueID()),
      &cancelable_task_tracker_);
}

void BackForwardMenuModel::OnFavIconDataAvailable(
    int navigation_entry_unique_id,
    const favicon_base::FaviconImageResult& image_result) {
  if (image_result.image.IsEmpty()) {
    return;
  }

  // Find the current model_index for the unique id.
  NavigationEntry* entry = nullptr;
  size_t model_index = 0;
  for (size_t i = 0; i + 1 < GetItemCount(); ++i) {
    if (IsSeparator(i)) {
      continue;
    }
    if (GetNavigationEntry(i)->GetUniqueID() == navigation_entry_unique_id) {
      model_index = i;
      entry = GetNavigationEntry(i);
      break;
    }
  }

  if (!entry) {
    // The NavigationEntry wasn't found, this can happen if the user
    // navigates to another page and a NavigatationEntry falls out of the
    // range of kMaxHistoryItems.
    return;
  }

  // Now that we have a valid NavigationEntry, decode the favicon and assign
  // it to the NavigationEntry.
  entry->GetFavicon().valid = true;
  entry->GetFavicon().url = image_result.icon_url;
  entry->GetFavicon().image = image_result.image;
  if (menu_model_delegate()) {
    menu_model_delegate()->OnIconChanged(GetCommandIdAt(model_index));
  }
}

size_t BackForwardMenuModel::GetHistoryItemCount() const {
  WebContents* contents = GetWebContents();
  if (!contents) {
    return 0;
  }

  size_t items = contents->GetController().GetCurrentEntryIndex();
  if (model_type_ == ModelType::kForward) {
    // Only count items from n+1 to end (if n is current entry)
    items = contents->GetController().GetEntryCount() - items - 1;
  }

  return std::min(items, kMaxHistoryItems);
}

size_t BackForwardMenuModel::GetChapterStopCount(size_t history_items) const {
  if (history_items != kMaxHistoryItems) {
    return 0;
  }

  WebContents* contents = GetWebContents();
  size_t current_entry = contents->GetController().GetCurrentEntryIndex();

  const bool forward = model_type_ == ModelType::kForward;
  size_t chapter_id = current_entry;
  if (!forward && chapter_id < history_items) {
    return 0;
  }
  chapter_id =
      forward ? (chapter_id + history_items) : (chapter_id - history_items);

  size_t chapter_stops = 0;
  do {
    const std::optional<size_t> index =
        GetIndexOfNextChapterStop(chapter_id, forward);
    if (!index.has_value()) {
      break;
    }
    chapter_id = index.value();
    ++chapter_stops;
  } while (chapter_stops < kMaxChapterStops);

  return chapter_stops;
}

size_t BackForwardMenuModel::GetSeparatorCount() const {
  size_t separator_count = 0;

  // Separator before chapter stops (if chapter stops exist)
  size_t history_items = GetHistoryItemCount();
  if (GetChapterStopCount(history_items) > 0) {
    separator_count += 1;
  }

  // Separator before "Show Full History" (if it exists)
  // "Show Full History" can exist when there are history items or
  // back-to-opener
  if (GetSectionItemCount(MenuSection::kShowFullHistory) > 0) {
    separator_count += 1;
  }

  return separator_count;
}

std::optional<size_t> BackForwardMenuModel::GetIndexOfNextChapterStop(
    size_t start_from,
    bool forward) const {
  // We want to advance over the current chapter stop, so we add one.
  // We don't need to do this when direction is backwards.
  if (forward) {
    start_from++;
  }

  NavigationController& controller = GetWebContents()->GetController();
  const size_t max_count = controller.GetEntryCount();
  if (start_from >= max_count) {
    return std::nullopt;  // Out of bounds.
  }

  NavigationEntry* start_entry = controller.GetEntryAtIndex(start_from);
  const GURL& url = start_entry->GetURL();

  auto same_domain_func = [&controller, &url](size_t i) {
    return net::registry_controlled_domains::SameDomainOrHost(
        url, controller.GetEntryAtIndex(i)->GetURL(),
        net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  };

  if (forward) {
    // When going forwards we return the entry before the entry that has a
    // different domain.
    for (size_t i = start_from + 1; i < max_count; ++i) {
      if (!same_domain_func(i)) {
        return i - 1;
      }
    }
    // Last entry is always considered a chapter stop.
    return max_count - 1;
  }

  // When going backwards we return the first entry we find that has a
  // different domain.
  for (size_t i = start_from; i > 0; --i) {
    if (!same_domain_func(i - 1)) {
      return i - 1;
    }
  }
  // We have reached the beginning without finding a chapter stop.
  return std::nullopt;
}

std::optional<size_t> BackForwardMenuModel::FindChapterStop(size_t offset,
                                                            bool forward,
                                                            size_t skip) const {
  WebContents* contents = GetWebContents();
  size_t entry = contents->GetController().GetCurrentEntryIndex();
  if (!forward && entry < offset) {
    return std::nullopt;
  }
  entry = forward ? (entry + offset) : (entry - offset);
  for (size_t i = 0; i <= skip; ++i) {
    const std::optional<size_t> index =
        GetIndexOfNextChapterStop(entry, forward);
    if (!index.has_value()) {
      return std::nullopt;
    }
    entry = index.value();
  }

  return entry;
}

std::u16string BackForwardMenuModel::GetShowFullHistoryLabel() const {
  return l10n_util::GetStringUTF16(IDS_HISTORY_SHOWFULLHISTORY_LINK);
}

WebContents* BackForwardMenuModel::GetWebContents() const {
  // We use the test web contents if the unit test has specified it.
  return test_web_contents_
             ? test_web_contents_.get()
             : browser_->tab_strip_model()->GetActiveWebContents();
}

std::optional<size_t> BackForwardMenuModel::MenuIndexToNavEntryIndex(
    size_t index) const {
  std::optional<MenuSection> section = GetSectionForIndex(index);
  if (!section.has_value()) {
    return std::nullopt;
  }

  switch (section.value()) {
    case MenuSection::kHistory: {
      // Convert history index to navigation entry index
      const size_t current_index =
          GetWebContents()->GetController().GetCurrentEntryIndex();
      const bool forward = model_type_ == ModelType::kForward;
      if (!forward && current_index <= index) {
        return std::nullopt;
      }
      return forward ? (current_index + index + 1)
                     : (current_index - (index + 1));
    }
    case MenuSection::kChapterStops: {
      // Calculate offset within chapter stops section.
      std::optional<size_t> chapter_start =
          GetStartingIndexOfSection(MenuSection::kChapterStops);
      CHECK(chapter_start.has_value());
      size_t offset_in_section = index - chapter_start.value();
      size_t history_items = GetHistoryItemCount();
      return FindChapterStop(history_items, model_type_ == ModelType::kForward,
                             offset_in_section);
    }
    case MenuSection::kBackToOpener:
    case MenuSection::kSeparator:
    case MenuSection::kShowFullHistory:
      // These sections don't have navigation entries.
      return std::nullopt;
  }
}

NavigationEntry* BackForwardMenuModel::GetNavigationEntry(size_t index) const {
  std::optional<size_t> controller_index = MenuIndexToNavEntryIndex(index);
  NavigationController& controller = GetWebContents()->GetController();

  DCHECK(controller_index.has_value());
  DCHECK_LT(controller_index.value(),
            static_cast<size_t>(controller.GetEntryCount()));

  return controller.GetEntryAtIndex(controller_index.value());
}

std::string BackForwardMenuModel::BuildActionName(
    const std::string& action,
    std::optional<size_t> index) const {
  DCHECK(!action.empty());
  std::string metric_string;
  if (model_type_ == ModelType::kForward) {
    metric_string += "ForwardMenu_";
  } else {
    metric_string += "BackMenu_";
  }
  metric_string += action;
  if (index.has_value()) {
    // +1 is for historical reasons (indices used to start at 1).
    metric_string += base::NumberToString(index.value() + 1);
  }
  return metric_string;
}

bool BackForwardMenuModel::ShouldShowFullHistoryBeVisible() const {
  return !browser_->profile()->IsOffTheRecord();
}
