// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/open_url_command_source.h"

#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/commander/fuzzy_finder.h"
#include "url/gurl.h"

namespace commander {

OpenURLCommandSource::OpenURLCommandSource() = default;
OpenURLCommandSource::~OpenURLCommandSource() = default;

CommandSource::CommandResults OpenURLCommandSource::GetCommands(
    const base::string16& input,
    Browser* browser) const {
  // TODO(lgrey): Strings are temporarily unlocalized since this is
  // experimental.
  static constexpr struct {
    const char* title;
    const char* url;
  } command_map[] = {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {"Chrome Help",
     "https://support.google.com/chrome/?p=help&ctx=menu#topic=9796470"},
    // GSuite
    {"New Google Doc", "https://docs.new"},
    {"New Google Sheet", "https://sheets.new"},
    {"New Google Slides", "https://slides.new"},
    {"New Google Form", "https://forms.new"},
    {"New Google Meet", "https://meet.new"},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  };

  CommandSource::CommandResults results;
  std::vector<gfx::Range> ranges;
  FuzzyFinder finder(input);
  for (const auto& command_spec : command_map) {
    base::string16 title = base::ASCIIToUTF16(command_spec.title);
    double score = finder.Find(title, &ranges);
    if (score == 0)
      continue;

    auto item = std::make_unique<CommandItem>(title, score, ranges);
    // base::Unretained is safe because commands are reset when a browser is
    // closed.
    item->command =
        base::BindOnce(&chrome::AddTabAt, base::Unretained(browser),
                       GURL(command_spec.url), -1, true, base::nullopt);
    results.push_back(std::move(item));
  }
  return results;
}

}  // namespace commander
