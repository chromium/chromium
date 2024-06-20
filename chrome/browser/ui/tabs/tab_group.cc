// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/tabs/tab_group_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/text_elider.h"
#include "url/gurl.h"

TabGroup::TabGroup(TabGroupController* controller,
                   const tab_groups::TabGroupId& id,
                   const tab_groups::TabGroupVisualData& visual_data)
    : controller_(controller), id_(id) {
  visual_data_ = std::make_unique<tab_groups::TabGroupVisualData>(visual_data);
}

TabGroup::~TabGroup() = default;

void TabGroup::SetVisualData(tab_groups::TabGroupVisualData visual_data,
                             bool is_customized) {
  // Move current visuals to old_visuals before updating
  std::unique_ptr<tab_groups::TabGroupVisualData> old_visuals =
      std::move(visual_data_);
  TabGroupChange::VisualsChange visuals;
  visuals.old_visuals = old_visuals.get();
  visuals.new_visuals = &visual_data;

  visual_data_ = std::make_unique<tab_groups::TabGroupVisualData>(visual_data);

  // Once the visual data is customized, it should stay customized.
  is_customized_ |= is_customized;

  // Notify the controller of the visual change
  controller_->ChangeTabGroupVisuals(id_, visuals);
}

std::u16string TabGroup::GetContentString() const {
  gfx::Range tabs_in_group = ListTabs();
  DCHECK_GT(tabs_in_group.length(), 0u);

  constexpr size_t kContextMenuTabTitleMaxLength = 30;
  std::u16string format_string = l10n_util::GetPluralStringFUTF16(
      IDS_TAB_CXMENU_PLACEHOLDER_GROUP_TITLE, tabs_in_group.length() - 1);
  std::u16string short_title;
  gfx::ElideString(controller_->GetTitleAt(tabs_in_group.start()),
                   kContextMenuTabTitleMaxLength, &short_title);
  return base::ReplaceStringPlaceholders(format_string, {short_title}, nullptr);
}

void TabGroup::AddTab() {
  if (tab_count_ == 0) {
    controller_->CreateTabGroup(id_);
    TabGroupChange::VisualsChange visuals;
    controller_->ChangeTabGroupVisuals(id_, visuals);
  }
  controller_->ChangeTabGroupContents(id_);
  ++tab_count_;
}

void TabGroup::RemoveTab() {
  DCHECK_GT(tab_count_, 0);
  --tab_count_;
  if (tab_count_ == 0)
    controller_->CloseTabGroup(id_);
  else
    controller_->ChangeTabGroupContents(id_);
}

bool TabGroup::IsEmpty() const {
  return tab_count_ == 0;
}

bool TabGroup::IsCustomized() const {
  return is_customized_;
}

std::optional<int> TabGroup::GetFirstTab() const {
  for (int i = 0; i < controller_->GetTabCount(); ++i) {
    if (controller_->GetTabGroupForTab(i) == id_)
      return i;
  }

  return std::nullopt;
}

std::optional<int> TabGroup::GetLastTab() const {
  for (int i = controller_->GetTabCount() - 1; i >= 0; --i) {
    if (controller_->GetTabGroupForTab(i) == id_)
      return i;
  }

  return std::nullopt;
}

gfx::Range TabGroup::ListTabs() const {
  std::optional<int> maybe_first_tab = GetFirstTab();
  if (!maybe_first_tab)
    return gfx::Range();

  int first_tab = maybe_first_tab.value();
  // Safe to assume GetLastTab() is not nullopt.
  int last_tab = GetLastTab().value();

  // If DCHECKs are enabled, check for group contiguity. The result
  // doesn't really make sense if the group is discontiguous.
  if (DCHECK_IS_ON()) {
    for (int i = first_tab; i <= last_tab; ++i) {
      DCHECK(controller_->GetTabGroupForTab(i) == id_);
    }
  }

  return gfx::Range(first_tab, last_tab + 1);
}
