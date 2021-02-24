// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COMMANDER_ENTITY_MATCH_H_
#define CHROME_BROWSER_UI_COMMANDER_ENTITY_MATCH_H_

#include <memory>
#include <vector>

#include "base/strings/string16.h"
#include "chrome/browser/ui/commander/command_source.h"
#include "ui/gfx/range/range.h"

class Browser;

namespace commander {

// Intermediate result type for browser windows that are eligible to be
// presented to the user as an option for a particular command.
struct WindowMatch {
  WindowMatch(Browser* browser, const base::string16& title, double score);
  ~WindowMatch();

  WindowMatch(WindowMatch&& other);
  WindowMatch& operator=(WindowMatch&& other);

  std::unique_ptr<CommandItem> ToCommandItem() const;

  Browser* browser;
  base::string16 title;
  std::vector<gfx::Range> matched_ranges;
  double score;
};

// Returns browser windows whose titles fuzzy match `input`. If input is empty,
// returns all eligible browser windows with score reflecting MRU order.
// `browser_to_exclude` is excluded from the list, as are all browser windows
// from a different profile unless `match_profile` is false.
std::vector<WindowMatch> WindowsMatchingInput(const Browser* browser_to_exclude,
                                              const base::string16& input,
                                              bool match_profile = false);
}  // namespace commander

#endif  // CHROME_BROWSER_UI_COMMANDER_ENTITY_MATCH_H_
