// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"

#include <memory>

#include "chrome/browser/bookmarks/url_and_id.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/model_type_store_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/common/channel_info.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_sync_bridge.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/model_type_store_service.h"
#include "content/public/browser/web_contents.h"

namespace {

std::unique_ptr<syncer::ClientTagBasedModelTypeProcessor>
CreateChangeProcessor() {
  return std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
      syncer::SAVED_TAB_GROUP,
      base::BindRepeating(&syncer::ReportUnrecoverableError,
                          chrome::GetChannel()));
}

}  // anonymous namespace

SavedTabGroupKeyedService::SavedTabGroupKeyedService(Profile* profile)
    : profile_(profile),
      listener_(model(), profile),
      bridge_(model(), GetStoreFactory(), CreateChangeProcessor()) {}

SavedTabGroupKeyedService::~SavedTabGroupKeyedService() = default;

syncer::OnceModelTypeStoreFactory SavedTabGroupKeyedService::GetStoreFactory() {
  DCHECK(ModelTypeStoreServiceFactory::GetForProfile(profile()));
  return ModelTypeStoreServiceFactory::GetForProfile(profile())
      ->GetStoreFactory();
}

void SavedTabGroupKeyedService::OpenSavedTabGroupInBrowser(
    Browser* browser,
    const base::GUID& saved_group_guid) {
  const SavedTabGroup* saved_group = model_.Get(saved_group_guid);

  // In the case where this function is called after confirmation of an
  // interstitial, the saved_group could be null, so protect against this by
  // early returning.
  if (!saved_group) {
    return;
  }

  // If the group already has a local group open, then activate it.
  if (saved_group->local_group_id().has_value()) {
    Browser* browser_for_activation = listener_.GetBrowserWithTabGroupId(
        saved_group->local_group_id().value());

    // Only activate the tab group's first tab if it exists in any browser's
    // tabstrip model.
    if (browser_for_activation) {
      absl::optional<int> first_tab =
          browser_for_activation->tab_strip_model()
              ->group_model()
              ->GetTabGroup(saved_group->local_group_id().value())
              ->GetFirstTab();
      DCHECK(first_tab.has_value());
      browser_for_activation->ActivateContents(
          browser_for_activation->tab_strip_model()->GetWebContentsAt(
              first_tab.value()));
      return;
    }
  }

  // If our tab group was not found in any tabstrip model, open the group in
  // this browser's tabstrip model.
  TabStripModel* tab_strip_model_for_creation = browser->tab_strip_model();

  std::vector<content::WebContents*> opened_web_contents;
  for (const SavedTabGroupTab& saved_tab : saved_group->saved_tabs()) {
    if (!saved_tab.url().is_valid()) {
      continue;
    }

    content::WebContents* created_contents =
        SavedTabGroupUtils::OpenTabInBrowser(
            saved_tab.url(), browser, profile_,
            WindowOpenDisposition::NEW_BACKGROUND_TAB);

    if (!created_contents) {
      continue;
    }

    base::Token token =
        listener_.GetOrCreateTrackedIDForWebContents(browser, created_contents);
    model_.Get(saved_tab.saved_group_guid())
        ->GetTab(saved_tab.saved_tab_guid())
        ->SetLocalTabID(token);

    opened_web_contents.emplace_back(created_contents);
  }

  // If no tabs were opened, then there's nothing to do.
  if (opened_web_contents.empty()) {
    return;
  }

  // Figure out which tabs we actually opened in this browser that aren't
  // already in groups.
  std::vector<int> tab_indices;
  for (int i = 0; i < tab_strip_model_for_creation->count(); ++i) {
    if (base::Contains(opened_web_contents,
                       tab_strip_model_for_creation->GetWebContentsAt(i)) &&
        !tab_strip_model_for_creation->GetTabGroupForTab(i).has_value()) {
      tab_indices.push_back(i);
    }
  }

  // Create a new group in the tabstrip.
  tab_groups::TabGroupId tab_group_id = tab_groups::TabGroupId::GenerateNew();
  tab_strip_model_for_creation->AddToGroupForRestore(tab_indices, tab_group_id);

  // Update the saved tab group to link to the local group id.
  model_.OnGroupOpenedInTabStrip(saved_group->saved_guid(), tab_group_id);

  TabGroup* const group =
      tab_strip_model_for_creation->group_model()->GetTabGroup(tab_group_id);

  // Activate the first tab in the tab group.
  absl::optional<int> first_tab = group->GetFirstTab();
  DCHECK(first_tab.has_value());
  tab_strip_model_for_creation->ActivateTabAt(first_tab.value());

  // Update the group to use the saved title and color.
  tab_groups::TabGroupVisualData visual_data(saved_group->title(),
                                             saved_group->color(),
                                             /*is_collapsed=*/false);
  // Set the groups visual data after the tab strip is in its final state. This
  // ensures the tab group's bounds are correctly set. crbug/1408814.
  group->SetVisualData(visual_data, /*is_customized=*/true);
}

void SavedTabGroupKeyedService::SaveGroup(
    const tab_groups::TabGroupId& group_id,
    Browser* browser) {
  Browser* browser_owning_tab_group =
      browser ? browser : listener_.GetBrowserWithTabGroupId(group_id);
  CHECK(browser_owning_tab_group);

  TabStripModel* tab_strip_model = browser_owning_tab_group->tab_strip_model();
  CHECK(tab_strip_model);
  CHECK(tab_strip_model->group_model());

  TabGroup* tab_group = tab_strip_model->group_model()->GetTabGroup(group_id);
  CHECK(tab_group);

  SavedTabGroup saved_tab_group(tab_group->visual_data()->title(),
                                tab_group->visual_data()->color(), {},
                                absl::nullopt, absl::nullopt, tab_group->id());

  // Build the SavedTabGroupTabs, track the webcontents, and add them to the
  // group.
  const gfx::Range tab_range = tab_group->ListTabs();
  for (auto i = tab_range.start(); i < tab_range.end(); ++i) {
    content::WebContents* web_contents = tab_strip_model->GetWebContentsAt(i);
    CHECK(web_contents);

    SavedTabGroupTab saved_tab_group_tab =
        SavedTabGroupUtils::CreateSavedTabGroupTabFromWebContents(
            web_contents, saved_tab_group.saved_guid());

    base::Token token = listener_.GetOrCreateTrackedIDForWebContents(
        browser_owning_tab_group, web_contents);
    saved_tab_group_tab.SetLocalTabID(token);

    saved_tab_group.AddTab(std::move(saved_tab_group_tab),
                           /*update_tab_positions=*/true);
  }

  model_.Add(std::move(saved_tab_group));
}

void SavedTabGroupKeyedService::UnsaveGroup(
    const tab_groups::TabGroupId& group_id) {
  // Get the guid since disconnect removes the local id.
  SavedTabGroup* group = model_.Get(group_id);
  CHECK(group);

  // Stop listening to the local group.
  DisconnectLocalTabGroup(group_id);

  // Unsave the group.
  model_.Remove(group->saved_guid());
}

void SavedTabGroupKeyedService::DisconnectLocalTabGroup(
    const tab_groups::TabGroupId& group_id) {
  Browser* browser_owning_tab_group =
      listener_.GetBrowserWithTabGroupId(group_id);
  CHECK(browser_owning_tab_group);

  TabStripModel* tab_strip_model = browser_owning_tab_group->tab_strip_model();
  CHECK(tab_strip_model);
  CHECK(tab_strip_model->group_model());

  TabGroup* tab_group = tab_strip_model->group_model()->GetTabGroup(group_id);
  CHECK(tab_group);

  // Stop listening to all of the webcontents in the group.
  const gfx::Range tab_range = tab_group->ListTabs();
  for (auto i = tab_range.start(); i < tab_range.end(); ++i) {
    content::WebContents* web_contents =
        browser_owning_tab_group->tab_strip_model()->GetWebContentsAt(i);
    listener_.StopTrackingWebContents(browser_owning_tab_group, web_contents);
  }

  SavedTabGroup* group = model_.Get(group_id);
  CHECK(group);

  // Stop listening to the current tab group.
  group->SetLocalGroupId(absl::nullopt);
}
