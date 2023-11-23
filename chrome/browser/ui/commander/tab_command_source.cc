// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/tab_command_source.h"

#include <numeric>
#include <string>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/commander/entity_match.h"
#include "chrome/browser/ui/commander/fuzzy_finder.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "components/sessions/content/session_tab_helper.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/list_selection_model.h"

namespace commander {

namespace {

// TODO(lgrey): It *might* make to pull this out later into a CommandSource
// method or a free function in some common place. Not committing yet.
std::unique_ptr<CommandItem> ItemForTitle(const std::u16string& title,
                                          FuzzyFinder& finder,
                                          std::vector<gfx::Range>* ranges) {
  double score = finder.Find(title, ranges);
  if (score > 0)
    return std::make_unique<CommandItem>(title, score, *ranges);
  return nullptr;
}

// Returns the tab group that the currently selected tabs can *not* be moved to.
// In practice, this is the tab group that *all* selected tabs belong to, if
// any. In the common special case of single selection, this will return that
// tab's group if it has one.
absl::optional<tab_groups::TabGroupId> IneligibleGroupForSelected(
    TabStripModel* tab_strip_model) {
  absl::optional<tab_groups::TabGroupId> excluded_group = absl::nullopt;
  for (int index : tab_strip_model->selection_model().selected_indices()) {
    auto group = tab_strip_model->GetTabGroupForTab(index);
    if (group.has_value()) {
      if (!excluded_group.has_value()) {
        excluded_group = group;
      } else if (group != excluded_group) {
        // More than one group in the selection, so don't exclude anything.
        return absl::nullopt;
      }
    }
  }
  return excluded_group;
}

// Returns true only if `browser` is alive, and the contents at `index` match
// `tab_session_id`.
bool DoesTabAtIndexMatchSessionId(base::WeakPtr<Browser> browser,
                                  int index,
                                  int tab_session_id) {
  if (!browser.get())
    return false;
  if (browser->tab_strip_model()->count() <= index)
    return false;
  content::WebContents* contents =
      browser->tab_strip_model()->GetWebContentsAt(index);
  DCHECK(contents);
  return sessions::SessionTabHelper::IdForTab(contents).id() == tab_session_id;
}

// Commands:

// TODO(lgrey): If this command ships, upstream these to TabStripModel
// (and get access to private methods for consistency).
bool CanCloseTabsToLeft(const TabStripModel* model) {
  const ui::ListSelectionModel& selection = model->selection_model();
  if (selection.empty())
    return false;
  int left_selected = *(selection.selected_indices().cbegin());
  for (int i = 0; i < left_selected; ++i) {
    if (!model->IsTabPinned(i))
      return true;
  }
  return false;
}

void CloseTabsToLeft(Browser* browser) {
  TabStripModel* model = browser->tab_strip_model();
  const ui::ListSelectionModel& selection = model->selection_model();
  if (selection.empty())
    return;
  int left_selected = *(selection.selected_indices().cbegin());
  for (int i = left_selected - 1; i >= 0; --i) {
    model->CloseWebContentsAt(i, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB |
                                     TabCloseTypes::CLOSE_USER_GESTURE);
  }
}

bool HasUnpinnedTabs(const TabStripModel* model) {
  return model->IndexOfFirstNonPinnedTab() < model->count();
}

bool HasPinnedTabs(const TabStripModel* model) {
  return model->IndexOfFirstNonPinnedTab() > 0;
}

void CloseUnpinnedTabs(Browser* browser) {
  TabStripModel* model = browser->tab_strip_model();
  for (int i = model->count() - 1; i >= 0; --i) {
    if (!model->IsTabPinned(i))
      model->CloseWebContentsAt(i, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB |
                                       TabCloseTypes::CLOSE_USER_GESTURE);
  }
}

bool CanMoveTabsToExistingWindow(const Browser* browser_to_exclude) {
  const BrowserList* browser_list = BrowserList::GetInstance();
  return base::ranges::any_of(
      *browser_list, [browser_to_exclude](Browser* browser) {
        return browser != browser_to_exclude && browser->is_type_normal() &&
               browser->profile() == browser_to_exclude->profile();
      });
}

void MoveTabsToExistingWindow(base::WeakPtr<Browser> source,
                              base::WeakPtr<Browser> target) {
  if (!source.get() || !target.get())
    return;
  const ui::ListSelectionModel::SelectedIndices& sel =
      source->tab_strip_model()->selection_model().selected_indices();
  chrome::MoveTabsToExistingWindow(source.get(), target.get(),
                                   std::vector<int>(sel.begin(), sel.end()));
}

bool CanAddAllToNewGroup(const TabStripModel* model) {
  return model->group_model()->ListTabGroups().size() == 0;
}

void AddAllToNewGroup(Browser* browser) {
  std::vector<int> indices(browser->tab_strip_model()->count());
  std::iota(indices.begin(), indices.end(), 0);
  browser->tab_strip_model()->AddToNewGroup(indices);
}

void AddSelectedToNewGroup(Browser* browser) {
  TabStripModel* model = browser->tab_strip_model();
  const ui::ListSelectionModel::SelectedIndices& sel =
      model->selection_model().selected_indices();
  model->AddToNewGroup(std::vector<int>(sel.begin(), sel.end()));
}

void MuteAllTabs(Browser* browser, bool exclude_active) {
  TabStripModel* model = browser->tab_strip_model();
  for (int i = 0; i < model->count(); ++i) {
    if (exclude_active && i == model->active_index())
      return;
    content::WebContents* contents = model->GetWebContentsAt(i);
    if (contents->IsCurrentlyAudible())
      contents->SetAudioMuted(true);
  }
}

// TODO(lgrey): Precalculate tab strip properties like "has audible tabs", "has
// pinned tabs" etc. in one iteration at search time.
bool HasAudibleTabs(const TabStripModel* model) {
  for (int i = 0; i < model->count(); ++i) {
    content::WebContents* contents = model->GetWebContentsAt(i);
    if (contents->IsCurrentlyAudible())
      return true;
  }
  return false;
}

bool HasMutedTabs(const TabStripModel* model) {
  for (int i = 0; i < model->count(); ++i) {
    content::WebContents* contents = model->GetWebContentsAt(i);
    if (contents->IsAudioMuted())
      return true;
  }
  return false;
}

void ScrollToTop(Browser* browser) {
  browser->tab_strip_model()->GetActiveWebContents()->ScrollToTopOfDocument();
}

void ScrollToBottom(Browser* browser) {
  browser->tab_strip_model()
      ->GetActiveWebContents()
      ->ScrollToBottomOfDocument();
}

// Multiphase commands:

void MuteUnmuteTab(base::WeakPtr<Browser> browser,
                   int tab_index,
                   int tab_session_id,
                   bool mute) {
  if (!DoesTabAtIndexMatchSessionId(browser, tab_index, tab_session_id))
    return;
  browser->tab_strip_model()->GetWebContentsAt(tab_index)->SetAudioMuted(mute);
}

std::unique_ptr<CommandItem> CreateMuteUnmuteTabItem(const TabMatch& match,
                                                     Browser* browser,
                                                     bool mute) {
  auto item = match.ToCommandItem();
  item->command = base::BindOnce(&MuteUnmuteTab, browser->AsWeakPtr(),
                                 match.index, match.session_id, mute);
  return item;
}

CommandSource::CommandResults MuteUnmuteTabItemsForTabsMatching(
    Browser* browser,
    bool mute,
    const std::u16string& input) {
  CommandSource::CommandResults results;
  TabSearchOptions options;
  if (mute)
    options.only_audible = true;
  else
    options.only_muted = true;
  for (auto& match : TabsMatchingInput(browser, input, options)) {
    results.push_back(CreateMuteUnmuteTabItem(match, browser, mute));
  }
  return results;
}

void TogglePinTab(base::WeakPtr<Browser> browser,
                  int tab_index,
                  int tab_session_id,
                  bool pin) {
  if (!DoesTabAtIndexMatchSessionId(browser, tab_index, tab_session_id))
    return;
  browser->tab_strip_model()->SetTabPinned(tab_index, pin);
}

std::unique_ptr<CommandItem> CreatePinTabItem(const TabMatch& match,
                                              Browser* browser,
                                              bool pin) {
  auto item = match.ToCommandItem();
  item->command = base::BindOnce(&TogglePinTab, browser->AsWeakPtr(),
                                 match.index, match.session_id, pin);
  return item;
}

CommandSource::CommandResults TogglePinTabCommandsForTabsMatching(
    Browser* browser,
    bool pin,
    const std::u16string& input) {
  CommandSource::CommandResults results;
  TabSearchOptions options;
  if (pin)
    options.only_unpinned = true;
  else
    options.only_pinned = true;
  for (auto& match : TabsMatchingInput(browser, input, options)) {
    results.push_back(CreatePinTabItem(match, browser, pin));
  }
  return results;
}

std::unique_ptr<CommandItem> CreateMoveTabsToWindowItem(
    Browser* source,
    const WindowMatch& match) {
  auto item = match.ToCommandItem();
  item->command = base::BindOnce(&MoveTabsToExistingWindow, source->AsWeakPtr(),
                                 match.browser->AsWeakPtr());
  return item;
}

CommandSource::CommandResults MoveTabsToWindowCommandsForWindowsMatching(
    Browser* source,
    const std::u16string& input) {
  CommandSource::CommandResults results;
  // Add "New Window", if appropriate. It should score highest with no input.
  std::u16string new_window_title = l10n_util::GetStringUTF16(IDS_NEW_WINDOW);
  std::erase(new_window_title, '&');
  std::unique_ptr<CommandItem> item;
  if (input.empty()) {
    item = std::make_unique<CommandItem>(new_window_title, .99,
                                         std::vector<gfx::Range>());
  } else {
    FuzzyFinder finder(input);
    std::vector<gfx::Range> ranges;
    item = ItemForTitle(new_window_title, finder, &ranges);
  }
  if (item) {
    item->entity_type = CommandItem::Entity::kWindow;
    item->command = base::BindOnce(&chrome::MoveActiveTabToNewWindow,
                                   base::Unretained(source));
    results.push_back(std::move(item));
  }
  for (auto& match : WindowsMatchingInput(source, input))
    results.push_back(CreateMoveTabsToWindowItem(source, match));
  return results;
}

void AddTabsToGroup(base::WeakPtr<Browser> browser,
                    tab_groups::TabGroupId group) {
  if (!browser.get())
    return;
  const ui::ListSelectionModel::SelectedIndices& sel =
      browser->tab_strip_model()->selection_model().selected_indices();
  browser->tab_strip_model()->AddToExistingGroup(
      std::vector<int>(sel.begin(), sel.end()), group);
}

CommandSource::CommandResults AddTabsToGroupCommandsForGroupsMatching(
    Browser* browser,
    const std::u16string& input) {
  CommandSource::CommandResults results;
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  // Add "New Group", if appropriate. It should score highest with no input.
  std::u16string new_group_title =
      l10n_util::GetStringUTF16(IDS_TAB_CXMENU_SUBMENU_NEW_GROUP);
  std::unique_ptr<CommandItem> item;
  if (input.empty()) {
    item = std::make_unique<CommandItem>(new_group_title, .99,
                                         std::vector<gfx::Range>());
  } else {
    FuzzyFinder finder(input);
    std::vector<gfx::Range> ranges;
    item = ItemForTitle(new_group_title, finder, &ranges);
  }
  if (item) {
    item->entity_type = CommandItem::Entity::kGroup;
    item->command =
        base::BindOnce(&AddSelectedToNewGroup, base::Unretained(browser));
    results.push_back(std::move(item));
  }
  for (auto& match : GroupsMatchingInput(
           browser, input, IneligibleGroupForSelected(tab_strip_model))) {
    auto command_item = match.ToCommandItem();
    command_item->command =
        base::BindOnce(&AddTabsToGroup, browser->AsWeakPtr(), match.group);
    results.push_back(std::move(command_item));
  }
  return results;
}

}  // namespace

TabCommandSource::TabCommandSource() = default;
TabCommandSource::~TabCommandSource() = default;

CommandSource::CommandResults TabCommandSource::GetCommands(
    const std::u16string& input,
    Browser* browser) const {
  CommandSource::CommandResults results;
  FuzzyFinder finder(input);
  std::vector<gfx::Range> ranges;
  ui::AcceleratorProvider* provider =
      chrome::AcceleratorProviderForBrowser(browser);

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  // TODO(lgrey): Temporarily using hardcoded English titles instead of
  // translated strings so we can experiment without adding translation load.
  if (auto item = ItemForTitle(u"Close current tab", finder, &ranges)) {
    item->command =
        base::BindOnce(&chrome::CloseTab, base::Unretained(browser));
    ui::Accelerator accelerator;
    if (provider->GetAcceleratorForCommandId(IDC_CLOSE_TAB, &accelerator))
      item->annotation = accelerator.GetShortcutText();
    results.push_back(std::move(item));
  }
  if (chrome::CanCloseOtherTabs(browser)) {
    if (auto item = ItemForTitle(u"Close other tabs", finder, &ranges)) {
      item->command =
          base::BindOnce(&chrome::CloseOtherTabs, base::Unretained(browser));
      results.push_back(std::move(item));
    }
  }
  if (chrome::CanCloseTabsToRight(browser)) {
    if (auto item = ItemForTitle(u"Close tabs to the right", finder, &ranges)) {
      item->command =
          base::BindOnce(&chrome::CloseTabsToRight, base::Unretained(browser));
      results.push_back(std::move(item));
    }
  }

  if (CanCloseTabsToLeft(tab_strip_model)) {
    if (auto item = ItemForTitle(u"Close tabs to the left", finder, &ranges)) {
      item->command =
          base::BindOnce(&CloseTabsToLeft, base::Unretained(browser));
      results.push_back(std::move(item));
    }
  }

  if (HasUnpinnedTabs(tab_strip_model)) {
    if (auto item = ItemForTitle(u"Close unpinned tabs", finder, &ranges)) {
      item->command =
          base::BindOnce(&CloseUnpinnedTabs, base::Unretained(browser));
      results.push_back(std::move(item));
    }
  }

  if (chrome::CanMoveActiveTabToNewWindow(browser)) {
    if (auto item =
            ItemForTitle(l10n_util::GetStringUTF16(IDS_MOVE_TAB_TO_NEW_WINDOW),
                         finder, &ranges)) {
      item->command = base::BindOnce(chrome::MoveActiveTabToNewWindow,
                                     base::Unretained(browser));
      results.push_back(std::move(item));
    }
  }

  if (CanMoveTabsToExistingWindow(browser)) {
    if (auto item = ItemForTitle(u"Move tabs to window...", finder, &ranges)) {
      item->command = std::make_pair(
          u"Move tabs to...",
          base::BindRepeating(&MoveTabsToWindowCommandsForWindowsMatching,
                              base::Unretained(browser)));
      results.push_back(std::move(item));
    }
  }

  if (CanAddAllToNewGroup(tab_strip_model)) {
    if (auto item =
            ItemForTitle(u"Move all tabs to new group", finder, &ranges)) {
      item->command =
          base::BindOnce(&AddAllToNewGroup, base::Unretained(browser));
      results.push_back(std::move(item));
    }
  }

  if (!tab_strip_model->WillContextMenuGroup(tab_strip_model->active_index())) {
    if (auto item = ItemForTitle(u"Ungroup tab", finder, &ranges)) {
      item->command =
          base::BindOnce(&chrome::GroupTab, base::Unretained(browser));
      results.push_back(std::move(item));
    }
  }
  if (auto item = ItemForTitle(u"Add tab to group...", finder, &ranges)) {
    item->command = std::make_pair(
        u"Add to group...",
        base::BindRepeating(&AddTabsToGroupCommandsForGroupsMatching,
                            base::Unretained(browser)));
    results.push_back(std::move(item));
  }
  if (auto item = ItemForTitle(u"Mute all tabs", finder, &ranges)) {
    item->command =
        base::BindOnce(&MuteAllTabs, base::Unretained(browser), false);
    results.push_back(std::move(item));
  }

  if (auto item = ItemForTitle(u"Mute other tabs", finder, &ranges)) {
    item->command =
        base::BindOnce(&MuteAllTabs, base::Unretained(browser), false);
    results.push_back(std::move(item));
  }

  if (HasAudibleTabs(tab_strip_model)) {
    if (auto item = ItemForTitle(u"Mute tab...", finder, &ranges)) {
      item->command =
          std::make_pair(u"Mute tab...",
                         base::BindRepeating(&MuteUnmuteTabItemsForTabsMatching,
                                             base::Unretained(browser), true));
      results.push_back(std::move(item));
    }
  }

  if (HasMutedTabs(tab_strip_model)) {
    if (auto item = ItemForTitle(u"Unmute tab...", finder, &ranges)) {
      item->command =
          std::make_pair(u"Unmute tab...",
                         base::BindRepeating(&MuteUnmuteTabItemsForTabsMatching,
                                             base::Unretained(browser), false));
      results.push_back(std::move(item));
    }
  }

  if (HasUnpinnedTabs(tab_strip_model)) {
    if (auto item = ItemForTitle(u"Pin tab...", finder, &ranges)) {
      item->command = std::make_pair(
          u"Pin tab...",
          base::BindRepeating(&TogglePinTabCommandsForTabsMatching,
                              base::Unretained(browser), true));
      results.push_back((std::move(item)));
    }
  }

  if (HasPinnedTabs(tab_strip_model)) {
    if (auto item = ItemForTitle(u"Unpin tab...", finder, &ranges)) {
      item->command = std::make_pair(
          u"Unpin tab...",
          base::BindRepeating(&TogglePinTabCommandsForTabsMatching,
                              base::Unretained(browser), false));
      results.push_back((std::move(item)));
    }
  }

  if (chrome::CanMoveActiveTabToReadLater(browser)) {
    if (auto item = ItemForTitle(u"Add to Read Later", finder, &ranges)) {
      item->command =
          base::BindOnce(IgnoreResult(&chrome::MoveCurrentTabToReadLater),
                         base::Unretained(browser));
      results.push_back(std::move(item));
    }
  }

  if (auto item = ItemForTitle(u"Scroll to top", finder, &ranges)) {
    item->command = base::BindOnce(&ScrollToTop, base::Unretained(browser));
    results.push_back(std::move(item));
  }

  if (auto item = ItemForTitle(u"Scroll to bottom", finder, &ranges)) {
    item->command = base::BindOnce(&ScrollToBottom, base::Unretained(browser));
    results.push_back(std::move(item));
  }

  if (send_tab_to_self::ShouldDisplayEntryPoint(
          tab_strip_model->GetActiveWebContents())) {
    if (auto item = ItemForTitle(u"Send tab to self...", finder, &ranges)) {
      item->command = base::BindOnce(&chrome::SendTabToSelfFromPageAction,
                                     base::Unretained(browser));
      results.push_back(std::move(item));
    }
  }

  return results;
}

}  // namespace commander
