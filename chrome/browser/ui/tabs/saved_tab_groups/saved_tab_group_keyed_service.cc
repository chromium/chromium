// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"

#include <memory>

#include "base/notreached.h"
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
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
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
      bridge_(model(), GetStoreFactory(), CreateChangeProcessor()) {
  model()->AddObserver(this);

  // Perform the necessary setup, if the model is already loaded before we start
  // observing it. Otherwise, the model will notify us when it has loaded.
  if (model()->is_loaded()) {
    SavedTabGroupModelLoaded();
  }
}

SavedTabGroupKeyedService::~SavedTabGroupKeyedService() {
  model_.RemoveObserver(this);
}

syncer::OnceModelTypeStoreFactory SavedTabGroupKeyedService::GetStoreFactory() {
  DCHECK(ModelTypeStoreServiceFactory::GetForProfile(profile()));
  return ModelTypeStoreServiceFactory::GetForProfile(profile())
      ->GetStoreFactory();
}

void SavedTabGroupKeyedService::StoreLocalToSavedId(
    const base::Uuid& saved_guid,
    const tab_groups::TabGroupId local_group_id) {
  CHECK(!model()->is_loaded());
  saved_guid_to_local_group_id_mapping_.emplace_back(saved_guid,
                                                     local_group_id);
}

void SavedTabGroupKeyedService::OpenSavedTabGroupInBrowser(
    Browser* browser,
    const base::Uuid& saved_group_guid) {
  const SavedTabGroup* saved_group = model_.Get(saved_group_guid);

  // In the case where this function is called after confirmation of an
  // interstitial, the saved_group could be null, so protect against this by
  // early returning.
  if (!saved_group) {
    return;
  }

  // If the group already has a local group open, then activate it.
  if (saved_group->local_group_id().has_value()) {
    Browser* browser_for_activation =
        SavedTabGroupUtils::GetBrowserWithTabGroupId(
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
  std::vector<std::pair<content::WebContents*, base::Uuid>>
      local_and_saved_tab_mapping;
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

    opened_web_contents.emplace_back(created_contents);
    local_and_saved_tab_mapping.emplace_back(created_contents,
                                             saved_tab.saved_tab_guid());
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

  TabGroup* const tab_group =
      tab_strip_model_for_creation->group_model()->GetTabGroup(tab_group_id);

  // Activate the first tab in the tab group.
  absl::optional<int> first_tab = tab_group->GetFirstTab();
  DCHECK(first_tab.has_value());
  tab_strip_model_for_creation->ActivateTabAt(first_tab.value());

  // Set the group's visual data after the tab strip is in its final state. This
  // ensures the tab group's bounds are correctly set. crbug/1408814.
  UpdateGroupVisualData(saved_group_guid,
                        saved_group->local_group_id().value());

  listener_.ConnectToLocalTabGroup(*model_.Get(saved_group_guid),
                                   local_and_saved_tab_mapping);
}

void SavedTabGroupKeyedService::SaveGroup(
    const tab_groups::TabGroupId& group_id) {
  Browser* browser = SavedTabGroupUtils::GetBrowserWithTabGroupId(group_id);
  CHECK(browser);

  TabStripModel* tab_strip_model = browser->tab_strip_model();
  CHECK(tab_strip_model);
  CHECK(tab_strip_model->group_model());

  TabGroup* tab_group = tab_strip_model->group_model()->GetTabGroup(group_id);
  CHECK(tab_group);

  SavedTabGroup saved_tab_group(tab_group->visual_data()->title(),
                                tab_group->visual_data()->color(), {},
                                absl::nullopt, absl::nullopt, tab_group->id());

  // Build the SavedTabGroupTabs and add them to the SavedTabGroup.
  const gfx::Range tab_range = tab_group->ListTabs();
  std::vector<std::pair<content::WebContents*, base::Uuid>>
      local_and_saved_tab_mapping;
  for (auto i = tab_range.start(); i < tab_range.end(); ++i) {
    content::WebContents* web_contents = tab_strip_model->GetWebContentsAt(i);
    CHECK(web_contents);

    SavedTabGroupTab saved_tab_group_tab =
        SavedTabGroupUtils::CreateSavedTabGroupTabFromWebContents(
            web_contents, saved_tab_group.saved_guid());

    local_and_saved_tab_mapping.emplace_back(
        web_contents, saved_tab_group_tab.saved_tab_guid());

    saved_tab_group.AddTab(std::move(saved_tab_group_tab),
                           /*update_tab_positions=*/true);
  }

  const base::Uuid saved_group_guid = saved_tab_group.saved_guid();
  model_.Add(std::move(saved_tab_group));

  // Link the local group to the saved group in the listener.
  listener_.ConnectToLocalTabGroup(*model_.Get(saved_group_guid),
                                   local_and_saved_tab_mapping);
}

void SavedTabGroupKeyedService::UnsaveGroup(
    const tab_groups::TabGroupId& group_id) {
  // Get the guid since disconnect removes the local id.
  const SavedTabGroup* group = model_.Get(group_id);
  CHECK(group);

  // Stop listening to the local group.
  DisconnectLocalTabGroup(group_id);

  // Unsave the group.
  model_.Remove(group->saved_guid());
}

void SavedTabGroupKeyedService::PauseTrackingLocalTabGroup(
    const tab_groups::TabGroupId& group_id) {
  listener_.PauseTrackingLocalTabGroup(group_id);
}

void SavedTabGroupKeyedService::ResumeTrackingLocalTabGroup(
    const base::Uuid& saved_group_guid,
    const tab_groups::TabGroupId& group_id) {
  listener_.ResumeTrackingLocalTabGroup(group_id);
  model_.OnGroupOpenedInTabStrip(saved_group_guid, group_id);
  UpdateGroupVisualData(saved_group_guid, group_id);
}

void SavedTabGroupKeyedService::DisconnectLocalTabGroup(
    const tab_groups::TabGroupId& group_id) {
  listener_.DisconnectLocalTabGroup(group_id);

  // Stop listening to the current tab group and notify observers.
  model_.OnGroupClosedInTabStrip(group_id);
}

void SavedTabGroupKeyedService::ConnectLocalTabGroup(
    const tab_groups::TabGroupId& local_group_id,
    const base::Uuid& saved_guid) {
  const TabStripModel* tab_strip_model =
      GetTabStripModelWithTabGroupId(local_group_id);
  TabGroup* const tab_group =
      tab_strip_model->group_model()->GetTabGroup(local_group_id);
  CHECK(tab_group);

  const gfx::Range& tab_range = tab_group->ListTabs();
  const SavedTabGroup* const saved_group = model_.Get(saved_guid);
  CHECK(saved_group);
  CHECK(tab_range.length() == saved_group->saved_tabs().size());

  std::vector<std::pair<content::WebContents*, base::Uuid>>
      web_contents_to_guid_mapping;

  for (size_t i = tab_range.start(); i < tab_range.end(); ++i) {
    content::WebContents* web_contents = tab_strip_model->GetWebContentsAt(i);
    CHECK(web_contents);

    const size_t saved_tab_index = i - tab_range.start();
    const SavedTabGroupTab& saved_tab =
        saved_group->saved_tabs()[saved_tab_index];

    web_contents_to_guid_mapping.emplace_back(web_contents,
                                              saved_tab.saved_tab_guid());
  }

  listener_.ConnectToLocalTabGroup(*model_.Get(saved_guid),
                                   std::move(web_contents_to_guid_mapping));

  UpdateGroupVisualData(saved_guid, local_group_id);
}

void SavedTabGroupKeyedService::SavedTabGroupModelLoaded() {
  for (const auto& [saved_guid, local_group_id] :
       saved_guid_to_local_group_id_mapping_) {
    model_.OnGroupOpenedInTabStrip(saved_guid, local_group_id);
    ConnectLocalTabGroup(local_group_id, saved_guid);
  }

  // Clear `saved_guid_to_local_group_id_mapping_` expecting that this observer
  // function will only be called once on startup freeing unsued space.
  //
  // TODO(dljames): Investigate using a single use callback to connect local and
  // saved groups together. There are crashes that occur when restarting the
  // browser before the browser process completely shuts down. This triggers the
  // CHECK in StoreLocalToSavedId because the SavedTabGroupModel has already
  // loaded. The callback will also remove the need of
  // `saved_guid_to_local_group_id_mapping_`.
  saved_guid_to_local_group_id_mapping_.clear();
  CHECK(saved_guid_to_local_group_id_mapping_.empty());
}

void SavedTabGroupKeyedService::SavedTabGroupUpdatedFromSync(
    const base::Uuid& group_guid,
    const absl::optional<base::Uuid>& tab_guid) {
  const SavedTabGroup* const saved_group = model_.Get(group_guid);
  CHECK(saved_group);

  // Do nothing if the saved group is not open in the tabstrip.
  if (!saved_group->local_group_id().has_value()) {
    return;
  }

  if (tab_guid.has_value()) {
    // TODO(dljames): Update tabs in the tabstrip if the respective group is
    // open with the updated tab metadata. Figure out if the tab should be
    // added, removed, or updated based on the data in saved_group.
    NOTIMPLEMENTED();
  } else {
    // Update the visual data of the saved group if it exists and is open in
    // the tabstrip.
    UpdateGroupVisualData(group_guid, saved_group->local_group_id().value());
  }
}

const TabStripModel* SavedTabGroupKeyedService::GetTabStripModelWithTabGroupId(
    const tab_groups::TabGroupId& local_group_id) {
  const Browser* const browser =
      SavedTabGroupUtils::GetBrowserWithTabGroupId(local_group_id);
  CHECK(browser);
  return browser->tab_strip_model();
}

void SavedTabGroupKeyedService::UpdateGroupVisualData(
    const base::Uuid saved_group_guid,
    const tab_groups::TabGroupId group_id) {
  TabGroup* const tab_group = SavedTabGroupUtils::GetTabGroupWithId(group_id);
  CHECK(tab_group);
  const SavedTabGroup* const saved_group = model_.Get(saved_group_guid);
  CHECK(saved_group);

  // Update the group to use the saved title and color.
  tab_groups::TabGroupVisualData visual_data(saved_group->title(),
                                             saved_group->color(),
                                             /*is_collapsed=*/false);
  tab_group->SetVisualData(visual_data, /*is_customized=*/true);
}
