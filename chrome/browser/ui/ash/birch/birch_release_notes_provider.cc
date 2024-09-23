// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/birch/birch_release_notes_provider.h"

#include <memory>
#include <vector>

#include "ash/birch/birch_item.h"
#include "ash/birch/birch_model.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/command_line.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/help_app_zero_state_provider.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

BirchReleaseNotesProvider::BirchReleaseNotesProvider(Profile* profile)
    : profile_(profile), release_notes_storage_(profile) {}

BirchReleaseNotesProvider::~BirchReleaseNotesProvider() = default;

void BirchReleaseNotesProvider::RequestBirchDataFetch() {
  if (release_notes_storage_.ShouldNotify()) {
    // Show birch if it's the first session after an update with release notes.
    // Since this flow updates the last milestone, it will not run again until
    // the next release with notes is available.
    first_seen_time_ = base::Time::Now();
    release_notes_storage_.MarkNotificationShown();
    release_notes_storage_.StartShowingSuggestionChip();
  }

  std::vector<BirchReleaseNotesItem> items;

  const bool force_show_release_notes =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceBirchReleaseNotes);

  // Control now falls onto ShouldShowSuggestionChip() and elapsed time because
  // number of times user has seen release notes surfaces and timer logic
  // outranks ShouldNotify() after first login.
  if (!force_show_release_notes &&
      (!release_notes_storage_.ShouldShowSuggestionChip() ||
       IsTimeframeToShowBirchEnded())) {
    Shell::Get()->birch_model()->SetReleaseNotesItems(std::move(items));
    return;
  }

  // TODO(b/325472224): Upgrade to V1, which includes dynamic feature titles
  // and images.
  items.emplace_back(
      l10n_util::GetStringUTF16(IDS_ASH_BIRCH_RELEASE_NOTES_TITLE),
      l10n_util::GetStringUTF16(IDS_ASH_BIRCH_RELEASE_NOTES_SUBTITLE),
      GURL("chrome://help-app/updates"),
      first_seen_time_.value_or(base::Time::Min()));

  Shell::Get()->birch_model()->SetReleaseNotesItems(std::move(items));
}

bool BirchReleaseNotesProvider::IsTimeframeToShowBirchEnded() {
  if (first_seen_time_.has_value()) {
    return base::Time::Now() - first_seen_time_.value() > base::Hours(24);
  }
  return true;
}

}  // namespace ash
