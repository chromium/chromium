// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/android_live_tab_context.h"

#include <memory>

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/restore_type.h"

AndroidLiveTabContext::AndroidLiveTabContext(TabModel* tab_model)
    : tab_model_(tab_model) {}

// Called in tab restore service, but expected to do nothing on Android.
void AndroidLiveTabContext::ShowBrowserWindow() {
}

SessionID AndroidLiveTabContext::GetSessionID() const {
  return tab_model_->GetSessionId();
}

int AndroidLiveTabContext::GetTabCount() const {
  return tab_model_->GetTabCount();
}

int AndroidLiveTabContext::GetSelectedIndex() const {
  return tab_model_->GetActiveIndex();
}

std::string AndroidLiveTabContext::GetAppName() const {
  // Not applicable to android.
  return std::string();
}

std::string AndroidLiveTabContext::GetUserTitle() const {
  return std::string();
}

sessions::LiveTab* AndroidLiveTabContext::GetLiveTabAt(int index) const {
  TabAndroid* tab_android = tab_model_->GetTabAt(index);
  if (!tab_android || !tab_android->web_contents())
    return nullptr;

  return sessions::ContentLiveTab::GetForWebContents(
      tab_android->web_contents());
}

sessions::LiveTab* AndroidLiveTabContext::GetActiveLiveTab() const {
  content::WebContents* web_contents = tab_model_->GetActiveWebContents();
  if (!web_contents)
    return nullptr;

  return sessions::ContentLiveTab::GetForWebContents(web_contents);
}

bool AndroidLiveTabContext::IsTabPinned(int index) const {
  // Not applicable to android.
  return false;
}

base::Optional<tab_groups::TabGroupId> AndroidLiveTabContext::GetTabGroupForTab(
    int index) const {
  // Not applicable to android.
  return base::Optional<tab_groups::TabGroupId>();
}

const tab_groups::TabGroupVisualData*
AndroidLiveTabContext::GetVisualDataForGroup(
    const tab_groups::TabGroupId& group) const {
  // Since we never return a group from GetTabGroupForTab(), this should never
  // be called.
  NOTREACHED();
  return nullptr;
}

void AndroidLiveTabContext::SetVisualDataForGroup(
    const tab_groups::TabGroupId& group,
    const tab_groups::TabGroupVisualData& group_visual_data) {
  // Not supported on Android.

  // TODO(crbug.com/1003128): ensure this never gets called (or remove
  // NOTREACHED) if we implement restoring groups for foreign session
  // windows.
  NOTREACHED();
}

const gfx::Rect AndroidLiveTabContext::GetRestoredBounds() const {
  // Not applicable to android.
  return gfx::Rect();
}

ui::WindowShowState AndroidLiveTabContext::GetRestoredState() const {
  // Not applicable to android.
  return ui::SHOW_STATE_NORMAL;
}

std::string AndroidLiveTabContext::GetWorkspace() const {
  // Not applicable to android.
  return std::string();
}

sessions::LiveTab* AndroidLiveTabContext::AddRestoredTab(
    const std::vector<sessions::SerializedNavigationEntry>& navigations,
    int tab_index,
    int selected_navigation,
    const std::string& extension_app_id,
    base::Optional<tab_groups::TabGroupId> group,
    const tab_groups::TabGroupVisualData& group_visual_data,
    bool select,
    bool pin,
    const sessions::PlatformSpecificTabData* tab_platform_data,
    const sessions::SerializedUserAgentOverride& user_agent_override,
    const SessionID* tab_id) {
  Profile* profile = tab_model_->GetProfile();

  // Prepare navigation history.
  std::vector<std::unique_ptr<content::NavigationEntry>> nav_entries =
        sessions::ContentSerializedNavigationBuilder::ToNavigationEntries(
            navigations, profile);

  // Restore web contents with navigation history.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(content::WebContents::CreateParams(profile));
  content::WebContents* raw_web_contents = web_contents.get();
  web_contents->GetController().Restore(
      selected_navigation, content::RestoreType::kRestored, &nav_entries);

  // Create new tab. Ownership is passed into java, which in turn creates a new
  // TabAndroid instance to own the WebContents.
  tab_model_->CreateTab(nullptr, web_contents.release());
  raw_web_contents->GetController().LoadIfNecessary();
  return sessions::ContentLiveTab::GetForWebContents(raw_web_contents);
}

// Currently does nothing.
sessions::LiveTab* AndroidLiveTabContext::ReplaceRestoredTab(
    const std::vector<sessions::SerializedNavigationEntry>& navigations,
    base::Optional<tab_groups::TabGroupId> group,
    int selected_navigation,
    const std::string& extension_app_id,
    const sessions::PlatformSpecificTabData* tab_platform_data,
    const sessions::SerializedUserAgentOverride& user_agent_override) {
  NOTIMPLEMENTED();
  return nullptr;
}

// Currently does nothing.
void AndroidLiveTabContext::CloseTab() {
  NOTIMPLEMENTED();
}

// static.
sessions::LiveTabContext* AndroidLiveTabContext::FindContextForWebContents(
    const content::WebContents* contents) {
  TabAndroid* tab_android = TabAndroid::FromWebContents(contents);
  if (!tab_android)
    return nullptr;

  TabModel* model = TabModelList::FindTabModelWithId(tab_android->window_id());

  return model ? model->GetLiveTabContext() : nullptr;
}

// static.
sessions::LiveTabContext* AndroidLiveTabContext::FindContextWithID(
    SessionID desired_id) {
  // Find the model with desired id.
  TabModel* tab_model = TabModelList::FindTabModelWithId(desired_id);

  // If we can't find the correct model, fall back to first non-incognito model.
  if (!tab_model || tab_model->IsOffTheRecord()) {
    for (const TabModel* model : TabModelList::models()) {
      if (!model->IsOffTheRecord()) {
        return model->GetLiveTabContext();
      }
    }
  }

  return tab_model ? tab_model->GetLiveTabContext() : nullptr;
}
