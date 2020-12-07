// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/simple_command_source.h"

#include "base/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/commander/fuzzy_finder.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"

namespace commander {

SimpleCommandSource::SimpleCommandSource() {
  weak_this_ = weak_ptr_factory_.GetWeakPtr();
}
SimpleCommandSource::~SimpleCommandSource() = default;

CommandSource::CommandResults SimpleCommandSource::GetCommands(
    const base::string16& input,
    Browser* browser) const {
  // TODO(lgrey): Temporarily using hardcoded English titles instead of
  // translated strings so we can experiment without adding translation load.
  // As implied, none of these strings are final, or necessarily expected to
  // ship.
  const struct {
    int id;
    base::string16 title;
  } command_map[] = {
      {IDC_FIND, l10n_util::GetStringUTF16(IDS_FIND)},
      {IDC_SAVE_PAGE, l10n_util::GetStringUTF16(IDS_SAVE_PAGE)},
      {IDC_PRINT, l10n_util::GetStringUTF16(IDS_PRINT)},
      {IDC_SHOW_HISTORY, base::ASCIIToUTF16("Show history")},
      {IDC_RELOAD, base::ASCIIToUTF16("Reload")},
      {IDC_NEW_TAB, base::ASCIIToUTF16("Create new tab")},
      {IDC_RESTORE_TAB, base::ASCIIToUTF16("Open recently closed tab")},
      {IDC_NEW_WINDOW, base::ASCIIToUTF16("Create new window")},
      {IDC_NEW_INCOGNITO_WINDOW,
       base::ASCIIToUTF16("Create new incognito window")},
      {IDC_BOOKMARK_THIS_TAB, base::ASCIIToUTF16("Bookmark this tab")},
      {IDC_BOOKMARK_ALL_TABS, base::ASCIIToUTF16("Bookmark all tabs")},
      {IDC_BACK, base::ASCIIToUTF16("Back")},
      {IDC_FORWARD, base::ASCIIToUTF16("Forward")},
      {IDC_ZOOM_PLUS, base::ASCIIToUTF16("Zoom in")},
      {IDC_ZOOM_MINUS, base::ASCIIToUTF16("Zoom out")},
      {IDC_ZOOM_NORMAL, base::ASCIIToUTF16("Reset zoom")},
      {IDC_VIEW_SOURCE, base::ASCIIToUTF16("View page source")},
      {IDC_EXIT, base::ASCIIToUTF16("Quit")},
      {IDC_EMAIL_PAGE_LOCATION, base::ASCIIToUTF16("Email page location")},
      {IDC_FOCUS_LOCATION, base::ASCIIToUTF16("Focus location bar")},
      {IDC_FOCUS_TOOLBAR, base::ASCIIToUTF16("Focus toolbar")},
      {IDC_OPEN_FILE, base::ASCIIToUTF16("Open file")},
      {IDC_TASK_MANAGER, base::ASCIIToUTF16("Show task manager")},
      {IDC_SHOW_BOOKMARK_MANAGER, base::ASCIIToUTF16("Show bookmark manager")},
      {IDC_SHOW_DOWNLOADS, base::ASCIIToUTF16("Show downloads")},
      {IDC_CLEAR_BROWSING_DATA, base::ASCIIToUTF16("Clear browsing data")},
      {IDC_OPTIONS, base::ASCIIToUTF16("Show settings")},
      {IDC_SHOW_AVATAR_MENU, base::ASCIIToUTF16("Switch profile")},
      {IDC_DEV_TOOLS_TOGGLE, base::ASCIIToUTF16("Toggle developer tools")},
  };
  CommandSource::CommandResults results;
  FuzzyFinder finder(input);
  std::vector<gfx::Range> ranges;
  for (const auto& command_spec : command_map) {
    if (!chrome::IsCommandEnabled(browser, command_spec.id))
      continue;
    base::string16 title = command_spec.title;
    base::Erase(title, '&');
    double score = finder.Find(title, &ranges);
    if (score == 0)
      continue;

    auto item = std::make_unique<CommandItem>();
    item->title = title;
    item->score = score;
    item->matched_ranges = ranges;

    ui::Accelerator accelerator;
    ui::AcceleratorProvider* provider =
        chrome::AcceleratorProviderForBrowser(browser);
    if (provider->GetAcceleratorForCommandId(command_spec.id, &accelerator)) {
      item->annotation = accelerator.GetShortcutText();
    }

    // base::Unretained is safe because the controller is reset when the
    // browser it's attached to closes.
    item->command =
        base::BindOnce(&SimpleCommandSource::ExecuteCommand, weak_this_,
                       base::Unretained(browser), command_spec.id);
    results.push_back(std::move(item));
  }

  return results;
}

// Why this is necessary:
// chrome::ExecuteCommand has a third default argument |time_stamp| which
// makes it difficult to use with BindOnce. Pre-binding it at command creation
// is wrong since it defaults to base::TimeTicks::Now(); that means if pre-bound
// it would get the timestamp when the command was generated, rather than when
// it was invoked.
void SimpleCommandSource::ExecuteCommand(Browser* browser, int command_id) {
  chrome::ExecuteCommand(browser, command_id);
}

}  // namespace commander
