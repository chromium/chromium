// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/window_command_source.h"

#include <numeric>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/commander/fuzzy_finder.h"

namespace commander {

namespace {

// Intermediate result type for browser windows that are eligible to be
// presented to the user as an option for a particular command.
struct WindowMatch {
  Browser* browser;
  base::string16 title;
  std::vector<gfx::Range> matched_ranges;
  double score;

  std::unique_ptr<CommandItem> ToCommandItem() const {
    auto item = std::make_unique<CommandItem>();
    item->title = title;
    item->entity_type = CommandItem::Entity::kWindow;
    item->score = score;
    item->matched_ranges = matched_ranges;
    return item;
  }
};

// TODO(lgrey): Specifically not deduping this with BookmarkCommandSource right
// now since I'm not actually sure if we want the same threshold for different
// nouns.
size_t constexpr kNounFirstMinimum = 2;

// TODO(lgrey): Just guessing for now! Not even sure if we need a max width,
// but right now, the code that does "<title> and x other tabs" wants a max.
double constexpr kMaxWidth = 1000;

// Activates `browser` if it's still present.
void SwitchToBrowser(base::WeakPtr<Browser> browser) {
  if (browser.get())
    browser->window()->Show();
}

// Merges all tabs from `source` into `target`, if they are both present.
void MergeBrowsers(base::WeakPtr<Browser> source,
                   base::WeakPtr<Browser> target) {
  if (!source.get() || !target.get())
    return;
  size_t source_count = source->tab_strip_model()->count();
  std::vector<int> indices(source_count);
  std::iota(indices.begin(), indices.end(), 0);
  chrome::MoveTabsToExistingWindow(source.get(), target.get(), indices);
}

// Returns browser windows whose titles fuzzy match `input`. If input is empty,
// returns all eligible browser windows with score reflecting MRU order.
// `browser_to_exclude` is excluded from the list, as are all browser windows
// from a different profile unless `match_profile` is false.
std::vector<WindowMatch> WindowsMatchingInput(const Browser* browser_to_exclude,
                                              const base::string16& input,
                                              bool match_profile = false) {
  std::vector<WindowMatch> results;
  const BrowserList* browser_list = BrowserList::GetInstance();
  double mru_score = 1.0;
  FuzzyFinder finder(input);
  std::vector<gfx::Range> ranges;
  for (BrowserList::const_reverse_iterator it =
           browser_list->begin_last_active();
       it != browser_list->end_last_active(); ++it) {
    Browser* browser = *it;
    if (browser == browser_to_exclude || !browser->is_type_normal())
      continue;
    if (match_profile && browser->profile() != browser_to_exclude->profile())
      continue;
    base::string16 title = browser->GetWindowTitleForMaxWidth(kMaxWidth);
    if (input.empty()) {
      WindowMatch match;
      match.browser = browser;
      match.title = std::move(title);
      match.score = mru_score;
      results.push_back(std::move(match));
      mru_score *= .95;
    } else {
      double score = finder.Find(title, &ranges);
      if (score > 0) {
        WindowMatch match;
        match.browser = browser;
        match.title = std::move(title);
        match.score = score;
        match.matched_ranges = ranges;
        results.push_back(std::move(match));
      }
    }
  }
  return results;
}

std::unique_ptr<CommandItem> CreateSwitchWindowItem(const WindowMatch& match) {
  auto item = match.ToCommandItem();
  item->command = base::BindOnce(&SwitchToBrowser, match.browser->AsWeakPtr());
  return item;
}

std::unique_ptr<CommandItem> CreateMergeWindowItem(Browser* source,
                                                   const WindowMatch& target) {
  auto item = target.ToCommandItem();
  item->command = base::BindOnce(&MergeBrowsers, source->AsWeakPtr(),
                                 target.browser->AsWeakPtr());
  return item;
}

CommandSource::CommandResults SwitchCommandsForWindowsMatching(
    Browser* browser_to_exclude,
    const base::string16& input) {
  CommandSource::CommandResults results;
  for (auto& match : WindowsMatchingInput(browser_to_exclude, input))
    results.push_back(CreateSwitchWindowItem(match));
  return results;
}

CommandSource::CommandResults MergeCommandsForWindowsMatching(
    Browser* source_browser,
    const base::string16& input) {
  CommandSource::CommandResults results;
  for (auto& match : WindowsMatchingInput(source_browser, input, true))
    results.push_back(CreateMergeWindowItem(source_browser, match));
  return results;
}

}  // namespace

WindowCommandSource::WindowCommandSource() = default;
WindowCommandSource::~WindowCommandSource() = default;

CommandSource::CommandResults WindowCommandSource::GetCommands(
    const base::string16& input,
    Browser* browser) const {
  CommandSource::CommandResults results;
  BrowserList* browser_list = BrowserList::GetInstance();
  if (browser_list->size() < 2)
    return results;
  if (input.size() >= kNounFirstMinimum) {
    results = SwitchCommandsForWindowsMatching(browser, input);
  }
  FuzzyFinder finder(input);
  std::vector<gfx::Range> ranges;
  // TODO(lgrey): Temporarily using untranslated strings since it's not
  // yet clear which commands will ship.
  base::string16 open_title = base::ASCIIToUTF16("Switch to window...");
  base::string16 merge_title =
      base::ASCIIToUTF16("Merge current window into...");

  double score = finder.Find(open_title, &ranges);
  if (score > 0) {
    auto verb = std::make_unique<CommandItem>();
    verb->title = open_title;
    verb->score = score;
    verb->matched_ranges = ranges;
    verb->command = std::make_pair(
        open_title, base::BindRepeating(&SwitchCommandsForWindowsMatching,
                                        base::Unretained(browser)));
    results.push_back(std::move(verb));
  }
  score = finder.Find(merge_title, &ranges);
  if (score > 0) {
    auto verb = std::make_unique<CommandItem>();
    verb->title = merge_title;
    verb->score = score;
    verb->matched_ranges = ranges;
    verb->command = std::make_pair(
        merge_title, base::BindRepeating(&MergeCommandsForWindowsMatching,
                                         base::Unretained(browser)));
    results.push_back(std::move(verb));
  }
  return results;
}
}  // namespace commander
