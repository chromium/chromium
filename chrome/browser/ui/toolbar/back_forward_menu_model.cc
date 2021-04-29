// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/numerics/ranges.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "components/favicon_base/favicon_types.h"
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
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/text_elider.h"

using base::UserMetricsAction;
using content::NavigationController;
using content::NavigationEntry;
using content::WebContents;

const int BackForwardMenuModel::kMaxHistoryItems = 12;
const int BackForwardMenuModel::kMaxChapterStops = 5;
static const int kMaxBackForwardMenuWidth = 700;

BackForwardMenuModel::BackForwardMenuModel(Browser* browser,
                                           ModelType model_type)
    : browser_(browser), model_type_(model_type) {}

BackForwardMenuModel::~BackForwardMenuModel() {}

bool BackForwardMenuModel::HasIcons() const {
  return true;
}

int BackForwardMenuModel::GetItemCount() const {
  int items = GetHistoryItemCount();
  if (items <= 0)
    return items;

  int chapter_stops = 0;

  // Next, we count ChapterStops, if any.
  if (items == kMaxHistoryItems)
    chapter_stops = GetChapterStopCount(items);

  if (chapter_stops)
    items += chapter_stops + 1;  // Chapter stops also need a separator.

  // If the menu is not empty, add two positions in the end
  // for a separator and a "Show Full History" item.
  items += 2;
  return items;
}

ui::MenuModel::ItemType BackForwardMenuModel::GetTypeAt(int index) const {
  return IsSeparator(index) ? TYPE_SEPARATOR : TYPE_COMMAND;
}

ui::MenuSeparatorType BackForwardMenuModel::GetSeparatorTypeAt(
    int index) const {
  return ui::NORMAL_SEPARATOR;
}

int BackForwardMenuModel::GetCommandIdAt(int index) const {
  return index;
}

std::u16string BackForwardMenuModel::GetLabelAt(int index) const {
  // Return label "Show Full History" for the last item of the menu.
  if (index == GetItemCount() - 1)
    return l10n_util::GetStringUTF16(IDS_HISTORY_SHOWFULLHISTORY_LINK);

  // Return an empty string for a separator.
  if (IsSeparator(index))
    return std::u16string();

  // Return the entry title, escaping any '&' characters and eliding it if it's
  // super long.
  NavigationEntry* entry = GetNavigationEntry(index);
  std::u16string menu_text(entry->GetTitleForDisplay());
  menu_text = ui::EscapeMenuLabelAmpersands(menu_text);
  menu_text = gfx::ElideText(menu_text, gfx::FontList(),
                             kMaxBackForwardMenuWidth, gfx::ELIDE_TAIL);

  return menu_text;
}

bool BackForwardMenuModel::IsItemDynamicAt(int index) const {
  // This object is only used for a single showing of a menu.
  return false;
}

bool BackForwardMenuModel::GetAcceleratorAt(
    int index,
    ui::Accelerator* accelerator) const {
  return false;
}

bool BackForwardMenuModel::IsItemCheckedAt(int index) const {
  return false;
}

int BackForwardMenuModel::GetGroupIdAt(int index) const {
  return false;
}

ui::ImageModel BackForwardMenuModel::GetIconAt(int index) const {
  if (!ItemHasIcon(index))
    return ui::ImageModel();

  if (index == GetItemCount() - 1) {
    return ui::ImageModel::FromImage(
        ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
            IDR_HISTORY_FAVICON));
  } else {
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
    return ui::ImageModel::FromImage(fav_icon.image);
  }
}

ui::ButtonMenuItemModel* BackForwardMenuModel::GetButtonMenuItemAt(
    int index) const {
  return nullptr;
}

bool BackForwardMenuModel::IsEnabledAt(int index) const {
  return index < GetItemCount() && !IsSeparator(index);
}

ui::MenuModel* BackForwardMenuModel::GetSubmenuModelAt(int index) const {
  return nullptr;
}

void BackForwardMenuModel::ActivatedAt(int index) {
  ActivatedAt(index, 0);
}

void BackForwardMenuModel::ActivatedAt(int index, int event_flags) {
  DCHECK(!IsSeparator(index));

  // Execute the command for the last item: "Show Full History".
  if (index == GetItemCount() - 1) {
    base::RecordComputedAction(BuildActionName("ShowFullHistory", -1));
    NavigateParams params(GetSingletonTabNavigateParams(
        browser_, GURL(chrome::kChromeUIHistoryURL)));
    ShowSingletonTabOverwritingNTP(browser_, &params);
    return;
  }

  // Log whether it was a history or chapter click.
  int items = GetHistoryItemCount();
  if (index < items) {
    base::RecordComputedAction(BuildActionName("HistoryClick", index));
  } else {
    const int chapter_index = index - items - 1;
    base::RecordComputedAction(BuildActionName("ChapterClick", chapter_index));
  }

  int controller_index = MenuIndexToNavEntryIndex(index);

  UMA_HISTOGRAM_BOOLEAN(
      "Navigation.BackForward.NavigatingToEntryMarkedToBeSkipped",
      GetWebContents()->GetController().IsEntryMarkedToBeSkipped(
          controller_index));

  WindowOpenDisposition disposition =
      ui::DispositionFromEventFlags(event_flags);
  chrome::NavigateToIndexWithDisposition(browser_, controller_index,
                                         disposition);
}

void BackForwardMenuModel::MenuWillShow() {
  base::RecordComputedAction(BuildActionName("Popup", -1));
  requested_favicons_.clear();
  cancelable_task_tracker_.TryCancelAll();
}

bool BackForwardMenuModel::IsSeparator(int index) const {
  int history_items = GetHistoryItemCount();
  // If the index is past the number of history items + separator,
  // we then consider if it is a chapter-stop entry.
  if (index > history_items) {
    // We either are in ChapterStop area, or at the end of the list (the "Show
    // Full History" link).
    int chapter_stops = GetChapterStopCount(history_items);
    if (chapter_stops == 0)
      return false;  // We must have reached the "Show Full History" link.
    // Otherwise, look to see if we have reached the separator for the
    // chapter-stops. If not, this is a chapter stop.
    return (index == history_items + 1 + chapter_stops);
  }

  // Look to see if we have reached the separator for the history items.
  return index == history_items;
}

void BackForwardMenuModel::FetchFavicon(NavigationEntry* entry) {
  // If the favicon has already been requested for this menu, don't do
  // anything.
  if (base::Contains(requested_favicons_, entry->GetUniqueID()))
    return;

  requested_favicons_.insert(entry->GetUniqueID());
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(browser_->profile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!favicon_service)
    return;

  favicon_service->GetFaviconImageForPageURL(
      entry->GetURL(),
      base::BindOnce(&BackForwardMenuModel::OnFavIconDataAvailable,
                     base::Unretained(this), entry->GetUniqueID()),
      &cancelable_task_tracker_);
}

void BackForwardMenuModel::OnFavIconDataAvailable(
    int navigation_entry_unique_id,
    const favicon_base::FaviconImageResult& image_result) {
  if (image_result.image.IsEmpty())
    return;

  // Find the current model_index for the unique id.
  NavigationEntry* entry = nullptr;
  int model_index = -1;
  for (int i = 0; i < GetItemCount() - 1; i++) {
    if (IsSeparator(i))
      continue;
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
    menu_model_delegate()->OnIconChanged(model_index);
  }
}

int BackForwardMenuModel::GetHistoryItemCount() const {
  WebContents* contents = GetWebContents();

  int items = contents->GetController().GetCurrentEntryIndex();
  if (model_type_ == ModelType::kForward) {
    // Only count items from n+1 to end (if n is current entry)
    items = contents->GetController().GetEntryCount() - items - 1;
  }

  return base::ClampToRange(items, 0, kMaxHistoryItems);
}

int BackForwardMenuModel::GetChapterStopCount(int history_items) const {
  if (history_items != kMaxHistoryItems)
    return 0;

  WebContents* contents = GetWebContents();
  int current_entry = contents->GetController().GetCurrentEntryIndex();

  const bool forward = model_type_ == ModelType::kForward;
  int chapter_id = current_entry;
  if (forward)
    chapter_id += history_items;
  else
    chapter_id -= history_items;

  int chapter_stops = 0;
  do {
    chapter_id = GetIndexOfNextChapterStop(chapter_id, forward);
    if (chapter_id == -1)
      break;
    ++chapter_stops;
  } while (chapter_stops < kMaxChapterStops);

  return chapter_stops;
}

int BackForwardMenuModel::GetIndexOfNextChapterStop(int start_from,
                                                    bool forward) const {
  if (start_from < 0)
    return -1;  // Out of bounds.

  // We want to advance over the current chapter stop, so we add one.
  // We don't need to do this when direction is backwards.
  if (forward)
    start_from++;

  NavigationController& controller = GetWebContents()->GetController();
  const int max_count = controller.GetEntryCount();
  if (start_from >= max_count)
    return -1;  // Out of bounds.

  NavigationEntry* start_entry = controller.GetEntryAtIndex(start_from);
  const GURL& url = start_entry->GetURL();

  auto same_domain_func = [&controller, &url](int i) {
    return net::registry_controlled_domains::SameDomainOrHost(
        url, controller.GetEntryAtIndex(i)->GetURL(),
        net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  };

  if (forward) {
    // When going forwards we return the entry before the entry that has a
    // different domain.
    for (int i = start_from + 1; i < max_count; ++i) {
      if (!same_domain_func(i))
        return i - 1;
    }
    // Last entry is always considered a chapter stop.
    return max_count - 1;
  }

  // When going backwards we return the first entry we find that has a
  // different domain.
  for (int i = start_from - 1; i >= 0; --i) {
    if (!same_domain_func(i))
      return i;
  }
  // We have reached the beginning without finding a chapter stop.
  return -1;
}

int BackForwardMenuModel::FindChapterStop(int offset,
                                          bool forward,
                                          int skip) const {
  if (offset < 0 || skip < 0)
    return -1;

  if (!forward)
    offset *= -1;

  WebContents* contents = GetWebContents();
  int entry = contents->GetController().GetCurrentEntryIndex() + offset;
  for (int i = 0; i < skip + 1; i++)
    entry = GetIndexOfNextChapterStop(entry, forward);

  return entry;
}

bool BackForwardMenuModel::ItemHasCommand(int index) const {
  return index < GetItemCount() && !IsSeparator(index);
}

bool BackForwardMenuModel::ItemHasIcon(int index) const {
  return index < GetItemCount() && !IsSeparator(index);
}

std::u16string BackForwardMenuModel::GetShowFullHistoryLabel() const {
  return l10n_util::GetStringUTF16(IDS_HISTORY_SHOWFULLHISTORY_LINK);
}

WebContents* BackForwardMenuModel::GetWebContents() const {
  // We use the test web contents if the unit test has specified it.
  return test_web_contents_ ?
      test_web_contents_ :
      browser_->tab_strip_model()->GetActiveWebContents();
}

int BackForwardMenuModel::MenuIndexToNavEntryIndex(int index) const {
  WebContents* contents = GetWebContents();
  int history_items = GetHistoryItemCount();

  DCHECK_GE(index, 0);

  // Convert anything above the History items separator.
  if (index < history_items) {
    if (model_type_ == ModelType::kForward) {
      index += contents->GetController().GetCurrentEntryIndex() + 1;
    } else {
      // Back menu is reverse.
      index = contents->GetController().GetCurrentEntryIndex() - (index + 1);
    }
    return index;
  }
  if (index == history_items)
    return -1;  // Don't translate the separator for history items.

  if (index >= history_items + 1 + GetChapterStopCount(history_items))
    return -1;  // This is beyond the last chapter stop so we abort.

  // This menu item is a chapter stop located between the two separators.
  return FindChapterStop(history_items, model_type_ == ModelType::kForward,
                         index - history_items - 1);
}

NavigationEntry* BackForwardMenuModel::GetNavigationEntry(int index) const {
  int controller_index = MenuIndexToNavEntryIndex(index);
  NavigationController& controller = GetWebContents()->GetController();

  DCHECK_GE(controller_index, 0);
  DCHECK_LT(controller_index, controller.GetEntryCount());

  return controller.GetEntryAtIndex(controller_index);
}

std::string BackForwardMenuModel::BuildActionName(
    const std::string& action, int index) const {
  DCHECK(!action.empty());
  DCHECK_GE(index, -1);
  std::string metric_string;
  if (model_type_ == ModelType::kForward)
    metric_string += "ForwardMenu_";
  else
    metric_string += "BackMenu_";
  metric_string += action;
  if (index != -1) {
    // +1 is for historical reasons (indices used to start at 1).
    metric_string += base::NumberToString(index + 1);
  }
  return metric_string;
}
