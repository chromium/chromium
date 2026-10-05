// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_tab_mute_menu_model.h"

#include <optional>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/i18n/rtl.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/notreached.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/menus/simple_menu_model.h"

namespace {

ui::ImageModel GetMuteIcon(bool will_mute) {
  const bool rounded = features::IsRoundedIconsEnabled();
  const gfx::VectorIcon& icon =
      will_mute ? (rounded ? vector_icons::kVolumeOffIcon
                           : vector_icons::kVolumeOffChromeRefreshOldIcon)
                : (rounded ? vector_icons::kVolumeUpIcon
                           : vector_icons::kVolumeUpChromeRefreshOldIcon);
  return ui::ImageModel::FromVectorIcon(icon, ui::kColorMenuIcon,
                                        ui::SimpleMenuModel::kDefaultIconSize);
}

}  // namespace

SplitTabMuteMenuModel::SplitTabMuteMenuModel(TabStripModel* tab_strip_model,
                                             int tab_index,
                                             split_tabs::SplitTabId split_id)
    : ui::SimpleMenuModel(this),
      tab_strip_model_(tab_strip_model),
      tab_index_(tab_index),
      split_id_(split_id) {
  AddItem(static_cast<int>(CommandId::kToggleAllSitesMuted), std::u16string());
  AddSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR);
  AddItem(static_cast<int>(CommandId::kToggleStartSiteMuted), std::u16string());
  AddItem(static_cast<int>(CommandId::kToggleEndSiteMuted), std::u16string());
}

SplitTabMuteMenuModel::~SplitTabMuteMenuModel() = default;

bool SplitTabMuteMenuModel::IsItemForCommandIdDynamic(int command_id) const {
  // Every item's label and icon depend on the current mute state.
  return true;
}

std::u16string SplitTabMuteMenuModel::GetLabelForCommandId(
    int command_id) const {
  const CommandId id = static_cast<CommandId>(command_id);
  const bool will_mute = WillMute(id);

  switch (id) {
    case CommandId::kToggleAllSitesMuted:
      return l10n_util::GetPluralStringFUTF16(
          will_mute ? IDS_TAB_CXMENU_SOUND_MUTE_SITE
                    : IDS_TAB_CXMENU_SOUND_UNMUTE_SITE,
          2);
    case CommandId::kToggleStartSiteMuted:
      if (IsSideBySide()) {
        if (base::i18n::IsRTL()) {
          return l10n_util::GetStringUTF16(
              will_mute ? IDS_SPLIT_TAB_MUTE_RIGHT_SITE
                        : IDS_SPLIT_TAB_UNMUTE_RIGHT_SITE);
        }
        return l10n_util::GetStringUTF16(will_mute
                                             ? IDS_SPLIT_TAB_MUTE_LEFT_SITE
                                             : IDS_SPLIT_TAB_UNMUTE_LEFT_SITE);
      }
      return l10n_util::GetStringUTF16(will_mute
                                           ? IDS_SPLIT_TAB_MUTE_TOP_SITE
                                           : IDS_SPLIT_TAB_UNMUTE_TOP_SITE);
    case CommandId::kToggleEndSiteMuted:
      if (IsSideBySide()) {
        if (base::i18n::IsRTL()) {
          return l10n_util::GetStringUTF16(
              will_mute ? IDS_SPLIT_TAB_MUTE_LEFT_SITE
                        : IDS_SPLIT_TAB_UNMUTE_LEFT_SITE);
        }
        return l10n_util::GetStringUTF16(will_mute
                                             ? IDS_SPLIT_TAB_MUTE_RIGHT_SITE
                                             : IDS_SPLIT_TAB_UNMUTE_RIGHT_SITE);
      }
      return l10n_util::GetStringUTF16(will_mute
                                           ? IDS_SPLIT_TAB_MUTE_BOTTOM_SITE
                                           : IDS_SPLIT_TAB_UNMUTE_BOTTOM_SITE);
  }
  NOTREACHED();
}

ui::ImageModel SplitTabMuteMenuModel::GetIconForCommandId(
    int command_id) const {
  return GetMuteIcon(WillMute(static_cast<CommandId>(command_id)));
}

bool SplitTabMuteMenuModel::IsCommandIdEnabled(int command_id) const {
  const CommandId id = static_cast<CommandId>(command_id);
  if (id == CommandId::kToggleAllSitesMuted) {
    return tab_strip_model_->IsContextMenuCommandEnabled(
        tab_index_, TabStripModel::CommandToggleSiteMuted);
  }

  // A site can only be muted once its tab has committed a navigation.
  const std::optional<int> index = GetTabIndexForCommand(id);
  return index.has_value() && !tab_strip_model_->GetWebContentsAt(*index)
                                   ->GetLastCommittedURL()
                                   .is_empty();
}

void SplitTabMuteMenuModel::ExecuteCommand(int command_id, int event_flags) {
  const CommandId id = static_cast<CommandId>(command_id);

  if (id == CommandId::kToggleAllSitesMuted) {
    // Route through the existing tab context menu command so that muting both
    // sites behaves and is logged exactly like the "Mute sites" item shown for
    // tabs that aren't in a split.
    tab_strip_model_->ExecuteContextMenuCommand(
        tab_index_, TabStripModel::CommandToggleSiteMuted);
    return;
  }

  // The split may have been removed while the menu was open.
  const std::optional<int> index = GetTabIndexForCommand(id);
  if (!index.has_value()) {
    return;
  }

  const bool mute = WillMute(id);
  base::RecordAction(
      base::UserMetricsAction(mute ? "SoundContentSetting.MuteBy.TabStrip"
                                   : "SoundContentSetting.UnmuteBy.TabStrip"));

  // This changes the sound content setting for the view's site, so it also
  // affects the other view when both views show the same site.
  tab_strip_model_->SetSitesMuted({*index}, mute);
}

std::optional<int> SplitTabMuteMenuModel::GetTabIndexForCommand(
    CommandId id) const {
  if (id == CommandId::kToggleAllSitesMuted ||
      !tab_strip_model_->ContainsSplit(split_id_)) {
    return std::nullopt;
  }
  const std::vector<tabs::TabInterface*> tabs_in_split =
      tab_strip_model_->GetSplitData(split_id_)->ListTabs();
  CHECK_EQ(tabs_in_split.size(), 2U);
  return tab_strip_model_->GetIndexOfTab(
      tabs_in_split[id == CommandId::kToggleStartSiteMuted ? 0 : 1]);
}

bool SplitTabMuteMenuModel::IsSideBySide() const {
  return !tab_strip_model_->ContainsSplit(split_id_) ||
         tab_strip_model_->GetSplitData(split_id_)
                 ->visual_data()
                 ->split_layout() == split_tabs::SplitTabLayout::kSideBySide;
}

bool SplitTabMuteMenuModel::WillMute(CommandId id) const {
  if (id == CommandId::kToggleAllSitesMuted) {
    return !tab_strip_model_->ContainsIndex(tab_index_) ||
           tab_strip_model_->WillContextMenuMuteSites(tab_index_);
  }
  const std::optional<int> index = GetTabIndexForCommand(id);
  return !index.has_value() || !IsSiteMuted(*tab_strip_model_, *index);
}
