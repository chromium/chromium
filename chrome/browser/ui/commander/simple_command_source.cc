// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/simple_command_source.h"

#include <vector>

#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/no_destructor.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/browser.h"
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
    const std::u16string& input,
    Browser* browser) const {
  // TODO(lgrey): Temporarily using hardcoded English titles instead of
  // translated strings so we can experiment without adding translation load.
  // As implied, none of these strings are final, or necessarily expected to
  // ship.
  struct CommandEntry {
    int id;
    std::u16string title;
  };
  static const base::NoDestructor<std::vector<CommandEntry>> kCommandMap({
      {IDC_FIND, l10n_util::GetStringUTF16(IDS_FIND)},
      {IDC_SAVE_PAGE, l10n_util::GetStringUTF16(IDS_SAVE_PAGE)},
      {IDC_PRINT, l10n_util::GetStringUTF16(IDS_PRINT)},
      {IDC_SHOW_HISTORY, u"Show history"},
      {IDC_RELOAD, u"Reload"},
      {IDC_NEW_TAB, u"Create new tab"},
      {IDC_RESTORE_TAB, u"Open recently closed tab"},
      {IDC_NEW_WINDOW, u"Create new window"},
      {IDC_NEW_INCOGNITO_WINDOW, u"Create new incognito window"},
      {IDC_BOOKMARK_THIS_TAB, u"Bookmark this tab"},
      {IDC_BOOKMARK_ALL_TABS, u"Bookmark all tabs"},
      {IDC_BACK, u"Back"},
      {IDC_FORWARD, u"Forward"},
      {IDC_ZOOM_PLUS, u"Zoom in"},
      {IDC_ZOOM_MINUS, u"Zoom out"},
      {IDC_ZOOM_NORMAL, u"Reset zoom"},
      {IDC_VIEW_SOURCE, u"View page source"},
      {IDC_EXIT, u"Quit"},
      {IDC_EMAIL_PAGE_LOCATION, u"Email page location"},
      {IDC_FOCUS_LOCATION, u"Focus location bar"},
      {IDC_FOCUS_TOOLBAR, u"Focus toolbar"},
      {IDC_OPEN_FILE, u"Open file"},
      {IDC_TASK_MANAGER, u"Show task manager"},
      {IDC_SHOW_BOOKMARK_MANAGER, u"Show bookmark manager"},
      {IDC_SHOW_DOWNLOADS, u"Show downloads"},
      {IDC_CLEAR_BROWSING_DATA, u"Clear browsing data"},
      {IDC_OPTIONS, u"Show settings"},
      {IDC_SHOW_AVATAR_MENU, u"Switch profile"},
      {IDC_DEV_TOOLS_TOGGLE, u"Toggle developer tools"},
      {IDC_MANAGE_EXTENSIONS, l10n_util::GetStringUTF16(IDS_MANAGE_EXTENSIONS)},
      {IDC_TAB_SEARCH, u"Search tabs..."},
      {IDC_SELECT_NEXT_TAB, u"Next tab"},
      {IDC_SELECT_PREVIOUS_TAB, u"Previous tab"},
      {IDC_MOVE_TAB_NEXT, u"Move tab forward"},
      {IDC_MOVE_TAB_PREVIOUS, u"Move tab backward"},
      {IDC_QRCODE_GENERATOR, u"Create QR code"},
  });

  CommandSource::CommandResults results;
  FuzzyFinder finder(input);
  std::vector<gfx::Range> ranges;
  for (const auto& command_spec : *kCommandMap) {
    if (!chrome::IsCommandEnabled(browser, command_spec.id))
      continue;
    std::u16string title = command_spec.title;
    base::Erase(title, '&');
    double score = finder.Find(title, &ranges);
    if (score == 0)
      continue;

    auto item = std::make_unique<CommandItem>(title, score, ranges);
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
