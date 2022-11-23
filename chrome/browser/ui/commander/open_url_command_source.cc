// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/open_url_command_source.h"

#include "base/i18n/case_conversion.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/commander/fuzzy_finder.h"
#include "chrome/grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

namespace commander {

namespace {

std::vector<std::pair<std::u16string, GURL>> CreateTitleURLMap() {
  return {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    {u"Chrome Help",
     GURL("https://support.google.com/chrome/?p=help&ctx=menu#topic=9796470")},
        // GSuite
        {u"New Google Doc", GURL("https://docs.new")},
        {u"New Google Sheet", GURL("https://sheets.new")},
        {u"New Google Slides", GURL("https://slides.new")},
        {u"New Google Form", GURL("https://forms.new")},
        {u"New Google Meet", GURL("https://meet.new")},
        {u"Open Theme Store",
         GURL(l10n_util::GetStringUTF8(IDS_THEMES_GALLERY_URL))},
        {u"Open Extension Store",
         GURL(l10n_util::GetStringUTF8(IDS_WEBSTORE_URL))},
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  };
}

}  // namespace

OpenURLCommandSource::OpenURLCommandSource()
    : title_url_map_(CreateTitleURLMap()) {}
OpenURLCommandSource::~OpenURLCommandSource() = default;

CommandSource::CommandResults OpenURLCommandSource::GetCommands(
    const std::u16string& input,
    Browser* browser) const {

  CommandSource::CommandResults results;
  std::vector<gfx::Range> ranges;
  FuzzyFinder finder(input);
  for (const auto& command_spec : title_url_map_) {
    std::u16string title = command_spec.first;
    double score = finder.Find(title, &ranges);
    if (score == 0)
      continue;

    auto item = std::make_unique<CommandItem>(title, score, ranges);
    // base::Unretained is safe because commands are reset when a browser is
    // closed.
    item->command =
        base::BindOnce(&chrome::AddTabAt, base::Unretained(browser),
                       command_spec.second, -1, true, absl::nullopt);
    results.push_back(std::move(item));
  }
  return results;
}

}  // namespace commander
