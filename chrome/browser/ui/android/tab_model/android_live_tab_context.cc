// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/android_live_tab_context.h"

#include <memory>
#include <optional>

#include "base/uuid.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/tab_restore_types.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/restore_type.h"
#include "ui/base/mojom/window_show_state.mojom.h"

AndroidLiveTabContext::AndroidLiveTabContext(TabModel* tab_model)
    : tab_model_(tab_model) {}

// Called in tab restore service, but expected to do nothing on Android.
void AndroidLiveTabContext::ShowBrowserWindow() {}

SessionID AndroidLiveTabContext::GetSessionID() const {
  return tab_model_->GetSessionId();
}

sessions::SessionWindow::WindowType AndroidLiveTabContext::GetWindowType()
    const {
  // Not applicable to android.
  return sessions::SessionWindow::TYPE_NORMAL;
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
  if (!tab_android || !tab_android->web_contents()) {
    return nullptr;
  }

  return sessions::ContentLiveTab::GetForWebContents(
      tab_android->web_contents());
}

sessions::LiveTab* AndroidLiveTabContext::GetActiveLiveTab() const {
  content::WebContents* web_contents = tab_model_->GetActiveWebContents();
  if (!web_contents) {
    return nullptr;
  }

  return sessions::ContentLiveTab::GetForWebContents(web_contents);
}

std::map<std::string, std::string> AndroidLiveTabContext::GetExtraDataForTab(
    int index) const {
  return std::map<std::string, std::string>();
}

std::map<std::string, std::string>
AndroidLiveTabContext::GetExtraDataForWindow() const {
  return std::map<std::string, std::string>();
}

std::optional<tab_groups::TabGroupId> AndroidLiveTabContext::GetTabGroupForTab(
    int index) const {
  // Implemented in AndroidLiveTabContextCloseWrapper.
  return std::optional<tab_groups::TabGroupId>();
}

const tab_groups::TabGroupVisualData*
AndroidLiveTabContext::GetVisualDataForGroup(
    const tab_groups::TabGroupId& group) const {
  // Implemented in AndroidLiveTabContextCloseWrapper.

  // Since we never return a group from GetTabGroupForTab(), this should never
  // be called.
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

const std::optional<base::Uuid>
AndroidLiveTabContext::GetSavedTabGroupIdForGroup(
    const tab_groups::TabGroupId& group) const {
  // Implemented in AndroidLiveTabContextCloseWrapper.
  return std::nullopt;
}

bool AndroidLiveTabContext::IsTabPinned(int index) const {
  // Not applicable to android.
  return false;
}

void AndroidLiveTabContext::SetVisualDataForGroup(
    const tab_groups::TabGroupId& group,
    const tab_groups::TabGroupVisualData& group_visual_data) {
  // Implemented in AndroidLiveTabContextRestoreWrapper.

  // TODO(crbug.com/40647050): ensure this never gets called (or remove
  // NOTREACHED) if we implement restoring groups for foreign session
  // windows.
  NOTREACHED_IN_MIGRATION();
}

const gfx::Rect AndroidLiveTabContext::GetRestoredBounds() const {
  // Not applicable to android.
  return gfx::Rect();
}

ui::mojom::WindowShowState AndroidLiveTabContext::GetRestoredState() const {
  // Not applicable to android.
  return ui::mojom::WindowShowState::kNormal;
}

std::string AndroidLiveTabContext::GetWorkspace() const {
  // Not applicable to android.
  return std::string();
}

sessions::LiveTab* AndroidLiveTabContext::AddRestoredTab(
    const sessions::tab_restore::Tab& tab,
    int tab_index,
    bool select,
    sessions::tab_restore::Type original_session_type) {
  Profile* profile = tab_model_->GetProfile();

  // Prepare navigation history.
  std::vector<std::unique_ptr<content::NavigationEntry>> nav_entries =
      sessions::ContentSerializedNavigationBuilder::ToNavigationEntries(
          tab.navigations, profile);

  // Restore web contents with navigation history. This is used for background
  // restore so start without a renderer.
  auto params = content::WebContents::CreateParams(profile);
  params.desired_renderer_state =
      content::WebContents::CreateParams::kNoRendererProcess;
  params.initially_hidden = true;
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(params);
  content::WebContents* raw_web_contents = web_contents.get();
  web_contents->GetController().Restore(tab.normalized_navigation_index(),
                                        content::RestoreType::kRestored,
                                        &nav_entries);

  // Create new tab. Ownership is passed into java, which in turn creates a new
  // TabAndroid instance to own the WebContents. Only select the restored tab
  // when restoring a single tab from a TAB session.
  tab_model_->CreateTab(
      nullptr, web_contents.release(),
      original_session_type == sessions::tab_restore::TAB ? true : false);
  // Don't load the tab yet. This prevents a renderer from starting which keeps
  // the tab restore lightweight as the tab is opened in the background only.
  // The tab will be in a "renderer was lost" state. This is recovered from when
  // the tab is made active.
  return sessions::ContentLiveTab::GetForWebContents(raw_web_contents);
}

sessions::LiveTab* AndroidLiveTabContext::ReplaceRestoredTab(
    const sessions::tab_restore::Tab& tab) {
  // Prepare navigation history.
  sessions::SessionTab session_tab;
  session_tab.current_navigation_index = tab.normalized_navigation_index();
  session_tab.navigations = tab.navigations;

  // This is called only on replacement of the current tab.
  content::WebContents* web_contents = tab_model_->GetActiveWebContents();
  web_contents = SessionRestore::RestoreForeignSessionTab(
      web_contents, session_tab, WindowOpenDisposition::CURRENT_TAB);
  web_contents->GetController().LoadIfNecessary();
  return sessions::ContentLiveTab::GetForWebContents(web_contents);
}

// Currently does nothing.
void AndroidLiveTabContext::CloseTab() {
  NOTIMPLEMENTED();
}

// static.
sessions::LiveTabContext* AndroidLiveTabContext::FindContextForWebContents(
    const content::WebContents* contents) {
  TabAndroid* tab_android = TabAndroid::FromWebContents(contents);
  if (!tab_android) {
    return nullptr;
  }

  TabModel* model =
      TabModelList::FindTabModelWithId(tab_android->GetWindowId());

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
