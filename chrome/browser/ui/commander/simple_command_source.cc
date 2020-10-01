// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/simple_command_source.h"

#include "base/bind.h"
#include "base/i18n/case_conversion.h"
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
  static constexpr struct {
    int command_id;
    int string_constant;
  } command_map[] = {
      {IDC_SHOW_HISTORY, IDS_HISTORY_SHOWFULLHISTORY_LINK},
      {IDC_FIND, IDS_FIND},
      {IDC_RELOAD, IDS_TOOLTIP_RELOAD},
      {IDC_SAVE_PAGE, IDS_SAVE_PAGE},
      {IDC_PRINT, IDS_PRINT},
  };

  CommandSource::CommandResults results;
  const base::string16& folded_input = base::i18n::FoldCase(input);
  std::vector<gfx::Range> ranges;
  for (const auto& command_spec : command_map) {
    if (!chrome::IsCommandEnabled(browser, command_spec.command_id))
      continue;
    base::string16 title =
        l10n_util::GetStringUTF16(command_spec.string_constant);
    base::Erase(title, '&');
    double score = FuzzyFind(folded_input, title, &ranges);
    if (score == 0)
      continue;

    auto item = std::make_unique<CommandItem>();
    item->title = title;
    item->score = score;
    item->matched_ranges = ranges;

    ui::Accelerator accelerator;
    ui::AcceleratorProvider* provider =
        chrome::AcceleratorProviderForBrowser(browser);
    if (provider->GetAcceleratorForCommandId(command_spec.command_id,
                                             &accelerator)) {
      item->annotation = accelerator.GetShortcutText();
    }

    // TODO(lgrey): For base::Unretained to be safe here, we need to ensure
    // that if |browser| is destroyed, the palette is reset. It's likely
    // that this will be the case anyway, but leaving this comment so:
    // - it doesn't get dropped/forgotten
    // - as a reminder to replace the comment with the actual explanation
    //   when we have it
    item->command =
        base::BindOnce(&SimpleCommandSource::ExecuteCommand, weak_this_,
                       base::Unretained(browser), command_spec.command_id);
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
