// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/entity_match.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/commander/fuzzy_finder.h"

namespace commander {

namespace {
// TODO(lgrey): Just guessing for now! Not even sure if we need a max width,
// but right now, the code that does "<title> and x other tabs" wants a max.
double constexpr kMaxTitleWidth = 1000;

}  // namespace

WindowMatch::WindowMatch(Browser* browser,
                         const base::string16& title,
                         double score)
    : browser(browser), title(title), score(score) {}
WindowMatch::~WindowMatch() = default;
WindowMatch::WindowMatch(WindowMatch&& other) = default;
WindowMatch& WindowMatch::operator=(WindowMatch&& other) = default;

std::unique_ptr<CommandItem> WindowMatch::ToCommandItem() const {
  auto item = std::make_unique<CommandItem>(title, score, matched_ranges);
  item->entity_type = CommandItem::Entity::kWindow;
  return item;
}

std::vector<WindowMatch> WindowsMatchingInput(const Browser* browser_to_exclude,
                                              const base::string16& input,
                                              bool match_profile) {
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
    base::string16 title = browser->GetWindowTitleForMaxWidth(kMaxTitleWidth);
    if (input.empty()) {
      WindowMatch match(browser, title, mru_score);
      results.push_back(std::move(match));
      mru_score *= .95;
    } else {
      double score = finder.Find(title, &ranges);
      if (score > 0) {
        WindowMatch match(browser, std::move(title), score);
        match.matched_ranges = ranges;
        results.push_back(std::move(match));
      }
    }
  }
  return results;
}

}  // namespace commander
