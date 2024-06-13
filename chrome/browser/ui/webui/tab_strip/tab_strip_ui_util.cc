// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_util.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/range/range.h"

namespace tab_strip_ui {

std::optional<tab_groups::TabGroupId> GetTabGroupIdFromString(
    TabGroupModel* tab_group_model,
    std::string group_id_string) {
  if (!tab_group_model)
    return std::nullopt;
  for (tab_groups::TabGroupId candidate : tab_group_model->ListTabGroups()) {
    if (candidate.ToString() == group_id_string) {
      return std::optional<tab_groups::TabGroupId>{candidate};
    }
  }

  return std::nullopt;
}

Browser* GetBrowserWithGroupId(Profile* profile, std::string group_id_string) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (profile && browser->profile() != profile) {
      continue;
    }

    std::optional<tab_groups::TabGroupId> group_id = GetTabGroupIdFromString(
        browser->tab_strip_model()->group_model(), group_id_string);
    if (group_id.has_value()) {
      return browser;
    }
  }

  return nullptr;
}

void MoveTabAcrossWindows(Browser* source_browser,
                          int from_index,
                          Browser* target_browser,
                          int to_index,
                          std::optional<tab_groups::TabGroupId> to_group_id) {
  bool was_active =
      source_browser->tab_strip_model()->active_index() == from_index;
  bool was_pinned = source_browser->tab_strip_model()->IsTabPinned(from_index);

  std::unique_ptr<tabs::TabModel> detached_tab =
      source_browser->tab_strip_model()->DetachTabAtForInsertion(from_index);

  int add_types = AddTabTypes::ADD_NONE;
  if (was_active) {
    add_types |= AddTabTypes::ADD_ACTIVE;
  }
  if (was_pinned) {
    add_types |= AddTabTypes::ADD_PINNED;
  }

  target_browser->tab_strip_model()->InsertDetachedTabAt(
      to_index, std::move(detached_tab), add_types, to_group_id);
}

bool IsDraggedTab(const ui::OSExchangeData& drop_data) {
  std::optional<base::Pickle> pickle = drop_data.GetPickledData(
      ui::ClipboardFormatType::DataTransferCustomType());
  if (!pickle.has_value()) {
    return false;
  }

  base::PickleIterator iter(pickle.value());
  uint32_t entry_count = 0;
  if (!iter.ReadUInt32(&entry_count)) {
    return false;
  }

  for (uint32_t i = 0; i < entry_count; ++i) {
    std::u16string_view type;
    std::u16string_view data;
    if (!iter.ReadStringPiece16(&type) || !iter.ReadStringPiece16(&data)) {
      return false;
    }

    if (type == kWebUITabIdDataType || type == kWebUITabGroupIdDataType) {
      return true;
    }
  }

  return false;
}

bool DropTabsInNewBrowser(Browser* new_browser,
                          const ui::OSExchangeData& drop_data) {
  std::u16string tab_id_str;
  std::u16string group_id_str;
  return ExtractTabData(drop_data, &tab_id_str, &group_id_str) &&
         DropTabsInNewBrowser(new_browser, tab_id_str, group_id_str);
}

bool DropTabsInNewBrowser(Browser* new_browser,
                          const std::u16string& tab_id_str,
                          const std::u16string& group_id_str) {
  if (tab_id_str.empty() && group_id_str.empty())
    return false;

  Browser* source_browser = nullptr;
  gfx::Range tab_indices_to_move;
  std::optional<tab_groups::TabGroupId> source_group_id;

  // TODO(crbug.com/40126106): de-duplicate with
  // TabStripUIHandler::HandleMoveTab and
  // TabStripUIHandler::HandleMoveGroup.

  if (!tab_id_str.empty()) {
    int tab_id = -1;
    if (!base::StringToInt(tab_id_str, &tab_id))
      return false;

    int source_index = -1;
    if (!extensions::ExtensionTabUtil::GetTabById(
            tab_id, new_browser->profile(), /* include_incognito = */ false,
            &source_browser, /* tab_strip = */ nullptr,
            /* contents = */ nullptr, &source_index)) {
      return false;
    }
    tab_indices_to_move = gfx::Range(source_index, source_index + 1);
  } else {
    std::string group_id_utf8 = base::UTF16ToUTF8(group_id_str);
    source_browser =
        GetBrowserWithGroupId(new_browser->profile(), group_id_utf8);
    if (!source_browser)
      return false;
    TabGroupModel* source_group_model =
        source_browser->tab_strip_model()->group_model();
    if (!source_group_model)
      return false;
    source_group_id =
        GetTabGroupIdFromString(source_group_model, group_id_utf8);
    if (!source_group_id)
      return false;
    TabGroup* source_group = source_group_model->GetTabGroup(*source_group_id);
    tab_indices_to_move = source_group->ListTabs();

    TabGroupModel* new_group_model =
        new_browser->tab_strip_model()->group_model();
    if (!new_group_model)
      return false;
    new_group_model->AddTabGroup(*source_group_id,
                                 *source_group->visual_data());
  }

  const int source_index = tab_indices_to_move.start();
  for (size_t i = 0; i < tab_indices_to_move.length(); ++i) {
    MoveTabAcrossWindows(source_browser, source_index, new_browser, i,
                         source_group_id);
  }
  new_browser->tab_strip_model()->ActivateTabAt(0);
  return true;
}

bool ExtractTabData(const ui::OSExchangeData& drop_data,
                    std::u16string* tab_id_str,
                    std::u16string* group_id_str) {
  DCHECK(tab_id_str);
  DCHECK(group_id_str);

  std::optional<base::Pickle> pickle = drop_data.GetPickledData(
      ui::ClipboardFormatType::DataTransferCustomType());
  if (!pickle.has_value()) {
    return false;
  }

  if (std::optional<std::u16string> maybe_tab_id =
          ui::ReadCustomDataForType(pickle.value(), kWebUITabIdDataType);
      maybe_tab_id && !maybe_tab_id->empty()) {
    *tab_id_str = std::move(*maybe_tab_id);
    return true;
  }

  if (std::optional<std::u16string> maybe_group_id =
          ui::ReadCustomDataForType(pickle.value(), kWebUITabGroupIdDataType);
      maybe_group_id && !maybe_group_id->empty()) {
    *group_id_str = std::move(*maybe_group_id);
    return true;
  }

  return false;
}

}  // namespace tab_strip_ui
