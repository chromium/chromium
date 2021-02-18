// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/tab_command_source.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/commander/fuzzy_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/models/list_selection_model.h"

namespace commander {

namespace {

// TODO(lgrey): It *might* make to pull this out later into a CommandSource
// method or a free function in some common place. Not committing yet.
std::unique_ptr<CommandItem> ItemForTitle(const base::string16& title,
                                          FuzzyFinder& finder,
                                          std::vector<gfx::Range>* ranges) {
  double score = finder.Find(title, ranges);
  if (score > 0)
    return std::make_unique<CommandItem>(title, score, *ranges);
  return nullptr;
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
    model->CloseWebContentsAt(i, TabStripModel::CLOSE_CREATE_HISTORICAL_TAB |
                                     TabStripModel::CLOSE_USER_GESTURE);
  }
}

bool CanCloseUnpinnedTabs(const TabStripModel* model) {
  for (int i = 0; i < model->count(); ++i) {
    if (!model->IsTabPinned(i))
      return true;
  }
  return false;
}

void CloseUnpinnedTabs(Browser* browser) {
  TabStripModel* model = browser->tab_strip_model();
  for (int i = model->count() - 1; i >= 0; --i) {
    if (!model->IsTabPinned(i))
      model->CloseWebContentsAt(i, TabStripModel::CLOSE_CREATE_HISTORICAL_TAB |
                                       TabStripModel::CLOSE_USER_GESTURE);
  }
}

}  // namespace

TabCommandSource::TabCommandSource() = default;
TabCommandSource::~TabCommandSource() = default;

CommandSource::CommandResults TabCommandSource::GetCommands(
    const base::string16& input,
    Browser* browser) const {
  CommandSource::CommandResults results;
  FuzzyFinder finder(input);
  std::vector<gfx::Range> ranges;
  ui::AcceleratorProvider* provider =
      chrome::AcceleratorProviderForBrowser(browser);

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  // TODO(lgrey): Temporarily using hardcoded English titles instead of
  // translated strings so we can experiment without adding translation load.
  if (auto item = ItemForTitle(base::ASCIIToUTF16("Close current tab"), finder,
                               &ranges)) {
    item->command =
        base::BindOnce(&chrome::CloseTab, base::Unretained(browser));
    ui::Accelerator accelerator;
    if (provider->GetAcceleratorForCommandId(IDC_CLOSE_TAB, &accelerator))
      item->annotation = accelerator.GetShortcutText();
    results.push_back(std::move(item));
  }
  if (chrome::CanCloseOtherTabs(browser)) {
    if (auto item = ItemForTitle(base::ASCIIToUTF16("Close other tabs"), finder,
                                 &ranges)) {
      item->command =
          base::BindOnce(&chrome::CloseOtherTabs, base::Unretained(browser));
      results.push_back(std::move(item));
    }
  }
  if (chrome::CanCloseTabsToRight(browser)) {
    if (auto item = ItemForTitle(base::ASCIIToUTF16("Close tabs to the right"),
                                 finder, &ranges)) {
      item->command =
          base::BindOnce(&chrome::CloseTabsToRight, base::Unretained(browser));
      results.push_back(std::move(item));
    }
  }

  if (CanCloseTabsToLeft(tab_strip_model)) {
    if (auto item = ItemForTitle(base::ASCIIToUTF16("Close tabs to the left"),
                                 finder, &ranges)) {
      item->command =
          base::BindOnce(&CloseTabsToLeft, base::Unretained(browser));
      results.push_back(std::move(item));
    }
  }

  if (CanCloseUnpinnedTabs(tab_strip_model)) {
    if (auto item = ItemForTitle(base::ASCIIToUTF16("Close unpinned tabs"),
                                 finder, &ranges)) {
      item->command =
          base::BindOnce(&CloseUnpinnedTabs, base::Unretained(browser));
      results.push_back(std::move(item));
    }
  }

  bool is_active_pinned =
      tab_strip_model->IsTabPinned(tab_strip_model->active_index());
  base::string16 active_title = is_active_pinned
                                    ? base::ASCIIToUTF16("Unpin tab")
                                    : base::ASCIIToUTF16("Pin tab");
  if (auto item = ItemForTitle(active_title, finder, &ranges)) {
    item->command = base::BindOnce(chrome::PinTab, base::Unretained(browser));
    results.push_back(std::move(item));
  }
  return results;
}

}  // namespace commander
