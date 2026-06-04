// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/side_panel_prefs.h"

#include "base/i18n/rtl.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"

namespace side_panel_prefs {

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
// TODO(crbug.com/489780965): Move policies over as features are implemented.
#if !BUILDFLAG(IS_ANDROID)
  // When in RTL mode, the side panel should default to the left of the screen.
  // Otherwise, the side panel should default to the right side of the screen.
  // TODO(dljames): Add enum values kAlternateSide / kDefaultSide that will
  // replace false and true respectively.
  registry->RegisterBooleanPref(prefs::kSidePanelHorizontalAlignment,
                                !base::i18n::IsRTL());
  registry->RegisterBooleanPref(prefs::kGoogleSearchSidePanelEnabled, true);
  registry->RegisterDictionaryPref(prefs::kSidePanelIdToWidth);

  base::DictValue alignment_overrides;
  alignment_overrides.Set(SidePanelEntryIdToString(SidePanelEntryId::kGlic),
                          !base::i18n::IsRTL());
  alignment_overrides.Set(
      SidePanelEntryIdToString(SidePanelEntryId::kContextualTasks),
      base::i18n::IsRTL());
  registry->RegisterDictionaryPref(prefs::kSidePanelAlignmentOverrides,
                                   std::move(alignment_overrides));
#endif
}

base::ListValue GetConfigurableSidePanelAlignments(Profile* profile) {
  base::ListValue panels;
#if !BUILDFLAG(IS_ANDROID)
  BrowserWindowInterface* browser =
      ProfileBrowserCollection::GetForProfile(profile)->FindTabbedBrowser();
  if (!browser) {
    return panels;
  }

  actions::ActionItem* root_action_item =
      browser->GetFeatures().GetRootActionItem();

  const base::DictValue& overrides =
      profile->GetPrefs()->GetDict(prefs::kSidePanelAlignmentOverrides);

  for (auto item : overrides) {
    std::optional<SidePanelEntryId> id = SidePanelEntryIdFromString(item.first);

    if (!id.has_value()) {
      continue;
    }

    std::optional<actions::ActionId> action_id =
        SidePanelEntryIdToActionId(*id);
    if (!action_id.has_value()) {
      continue;
    }
    actions::ActionItem* action_item =
        actions::ActionManager::Get().FindAction(*action_id, root_action_item);
    if (!action_item || !action_item->GetVisible()) {
      continue;
    }

    base::DictValue panel_data;
    panel_data.Set("id", item.first);

    std::u16string label;
    // By default use the ActionItem text for the label, but features can
    // override that here.
    switch (*id) {
      case SidePanelEntryId::kContextualTasks:
        label = l10n_util::GetStringUTF16(
            IDS_SETTINGS_SIDE_PANEL_ALIGNMENT_CONTEXTUAL_TASKS);
        break;
      default:
        label = std::u16string(action_item->GetText());
        break;
    }
    panel_data.Set("label", label);
    panels.Append(std::move(panel_data));
  }
#endif
  return panels;
}

}  // namespace side_panel_prefs
