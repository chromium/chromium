// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_view_controller.h"

#include <set>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_model.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_prefs.h"
#include "chrome/browser/ui/toolbar/chrome_labs/chrome_labs_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_bubble_view.h"
#include "chrome/browser/ui/views/toolbar/chrome_labs/chrome_labs_item_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/buildflags.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/flags_state.h"
#include "components/flags_ui/flags_storage.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_education/common/new_badge_controller.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/settings/about_flags.h"
#include "chrome/browser/profiles/profile.h"
#else
#include "chrome/browser/browser_process.h"
#endif

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ChromeLabsSelectedLab {
  kUnspecifiedSelected = 0,
  // kReadLaterSelected = 1,
  // kTabSearchSelected = 2,
  kTabScrollingSelected = 3,
  // kSidePanelSelected = 4,
  // kLensRegionSearchSelected = 5,
  kWebUITabStripSelected = 6,
  // kTabSearchMediaTabsSelected = 7,
  // kChromeRefresh2023Selected = 8,
  // kTabGroupsSaveSelected = 9,
  // kChromeWebuiRefresh2023Selected = 10,
  kCustomizeChromeSidePanelSelected = 11,
  kMaxValue = kCustomizeChromeSidePanelSelected,
};

void EmitToHistogram(const std::u16string& selected_lab_state,
                     const std::string& internal_name) {
  const auto get_histogram_name = [](const std::u16string& selected_lab_state) {
    if (selected_lab_state ==
        base::ASCIIToUTF16(flags_ui::kGenericExperimentChoiceDefault)) {
      return "Toolbar.ChromeLabs.DefaultLabAction";
    }
    if (selected_lab_state ==
        base::ASCIIToUTF16(flags_ui::kGenericExperimentChoiceEnabled)) {
      return "Toolbar.ChromeLabs.EnableLabAction";
    }
    if (selected_lab_state ==
        base::ASCIIToUTF16(flags_ui::kGenericExperimentChoiceDisabled)) {
      return "Toolbar.ChromeLabs.DisableLabAction";
    }
    return "";
  };

  const auto get_enum = [](const std::string& internal_name) {
    if (internal_name == flag_descriptions::kScrollableTabStripFlagId)
      return ChromeLabsSelectedLab::kTabScrollingSelected;
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP) && \
    (BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH))
    if (internal_name == flag_descriptions::kWebUITabStripFlagId)
      return ChromeLabsSelectedLab::kWebUITabStripSelected;
#endif

    return ChromeLabsSelectedLab::kUnspecifiedSelected;
  };

  const std::string histogram_name = get_histogram_name(selected_lab_state);
  if (!histogram_name.empty())
    base::UmaHistogramEnumeration(histogram_name, get_enum(internal_name));
}

// Returns the number of days since epoch (1970-01-01) in the local timezone.
uint32_t GetCurrentDay() {
  base::TimeDelta delta = base::Time::Now() - base::Time::UnixEpoch();
  return base::saturated_cast<uint32_t>(delta.InDays());
}

}  // namespace

ChromeLabsViewController::ChromeLabsViewController(
    const ChromeLabsModel* model,
    ChromeLabsBubbleView* chrome_labs_bubble_view,
    Browser* browser,
    flags_ui::FlagsState* flags_state,
    flags_ui::FlagsStorage* flags_storage)
    : model_(model),
      chrome_labs_bubble_view_(chrome_labs_bubble_view),
      browser_(browser),
      flags_state_(flags_state),
      flags_storage_(flags_storage) {
  ParseModelDataAndAddLabs();
  SetRestartCallback();
}

int ChromeLabsViewController::GetIndexOfEnabledLabState(
    const flags_ui::FeatureEntry* entry,
    flags_ui::FlagsState* flags_state,
    flags_ui::FlagsStorage* flags_storage) {
  std::set<std::string> enabled_entries;
  flags_state->GetSanitizedEnabledFlags(flags_storage, &enabled_entries);
  for (int i = 0; i < entry->NumOptions(); i++) {
    const std::string name = entry->NameForOption(i);
    if (base::Contains(enabled_entries, name))
      return i;
  }
  return 0;
}

void ChromeLabsViewController::ParseModelDataAndAddLabs() {
  // Create each lab item.
  const std::vector<LabInfo>& all_labs = model_->GetLabInfo();
  for (const auto& lab : all_labs) {
    const flags_ui::FeatureEntry* entry =
        flags_state_->FindFeatureEntryByName(lab.internal_name);
    if (IsChromeLabsFeatureValid(lab, browser_->profile())) {
      bool valid_entry_type =
          entry->type == flags_ui::FeatureEntry::FEATURE_VALUE ||
          entry->type == flags_ui::FeatureEntry::FEATURE_WITH_PARAMS_VALUE;
      DCHECK(valid_entry_type);
      int default_index =
          GetIndexOfEnabledLabState(entry, flags_state_, flags_storage_);
      ChromeLabsItemView* lab_item = chrome_labs_bubble_view_->AddLabItem(
          lab, default_index, entry, browser_,
          base::BindRepeating(
              [](ChromeLabsBubbleView* bubble_view, std::string internal_name,
                 flags_ui::FlagsStorage* flags_storage,
                 ChromeLabsItemView* item_view) {
                size_t selected_index = item_view->GetSelectedIndex().value();
                about_flags::SetFeatureEntryEnabled(
                    flags_storage,
                    internal_name + flags_ui::kMultiSeparatorChar +
                        base::NumberToString(selected_index),
                    true);

                bubble_view->ShowRelaunchPrompt();
                EmitToHistogram(
                    item_view->GetFeatureEntry()->DescriptionForOption(
                        selected_index),
                    internal_name);
              },
              chrome_labs_bubble_view_.get(), lab.internal_name,
              flags_storage_));
      lab_item->SetShowNewBadge(
          ShouldLabShowNewBadge(browser_->profile(), lab));
    }
  }
}

void ChromeLabsViewController::RestartToApplyFlags() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS be less intrusive and restart inside the user session after
  // we apply the newly selected flags.
  VLOG(1) << "Restarting to apply per-session flags...";
  ash::about_flags::FeatureFlagsUpdate(
      *flags_storage_, browser_->profile()->GetOriginalProfile()->GetPrefs())
      .UpdateSessionManager();
#endif
  // During the restart process some situations may cause previously active
  // bubbles to deactivate. Since the restart action itself is not binded to any
  // state, run the restart asynchronously. See crbug.com/1310212 where
  // deactivation of bubbles is caused by the modal for downloads in progress
  // being shown.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&chrome::AttemptRestart));
}

void ChromeLabsViewController::SetRestartCallback() {
  restart_callback_ = chrome_labs_bubble_view_->RegisterRestartCallback(
      base::BindRepeating(&ChromeLabsViewController::RestartToApplyFlags,
                          base::Unretained(this)));
}

user_education::DisplayNewBadge ChromeLabsViewController::ShouldLabShowNewBadge(
    Profile* profile,
    const LabInfo& lab) {
  // This experiment was added before adding the new badge and is not new.
  if (lab.internal_name == flag_descriptions::kScrollableTabStripFlagId) {
    return user_education::DisplayNewBadge();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ScopedDictPrefUpdate update(
      profile->GetPrefs(), chrome_labs_prefs::kChromeLabsNewBadgeDictAshChrome);
#else
  ScopedDictPrefUpdate update(g_browser_process->local_state(),
                              chrome_labs_prefs::kChromeLabsNewBadgeDict);
#endif

  base::Value::Dict& new_badge_prefs = update.Get();
  std::optional<int> start_day = new_badge_prefs.FindInt(lab.internal_name);
  DCHECK(start_day);
  uint32_t current_day = GetCurrentDay();
  if (*start_day == chrome_labs_prefs::kChromeLabsNewExperimentPrefValue) {
    // Set the dictionary value of this experiment to the number of days since
    // epoch (1970-01-01). This value is the first day the user sees the new
    // experiment in Chrome Labs and will be used to determine whether or not to
    // show the new badge.
    new_badge_prefs.Set(lab.internal_name, static_cast<int>(current_day));
    return user_education::DisplayNewBadge(
        base::PassKey<ChromeLabsViewController>(), true);
  }
  int days_elapsed = current_day - *start_day;
  // Show the new badge for 7 days. If the users sets the clock such that the
  // current day is now before |start_day| don’t show the new badge.
  return user_education::DisplayNewBadge(
      base::PassKey<ChromeLabsViewController>(),
      days_elapsed >= 0 && days_elapsed < 7);
}

void ChromeLabsViewController::RestartToApplyFlagsForTesting() {
  RestartToApplyFlags();
}
