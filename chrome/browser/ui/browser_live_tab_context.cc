// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_live_tab_context.h"

#include <memory>
#include <utility>

#include "base/token.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabrestore.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_group_id.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/content/content_platform_specific_tab_data.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/session_storage_namespace.h"
#include "extensions/browser/extension_registry.h"

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/tab_loader.h"
#endif

using content::NavigationController;
using content::SessionStorageNamespace;
using content::WebContents;

namespace {

// |app_name| can could be for an app that has been uninstalled. In that
// case we don't want to open an app window. Note that |app_name| is also used
// for other types of windows like dev tools and we always want to open an
// app window in those cases.
bool ShouldCreateAppWindowForAppName(Profile* profile,
                                     const std::string& app_name) {
  if (app_name.empty())
    return false;

  // Only need to check that the app is installed if |app_name| is for an
  // extension. (|app_name| could also be for a devtools windows.)
  const std::string app_id = web_app::GetAppIdFromApplicationName(app_name);
  if (app_id.empty())
    return true;

  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          app_id);
  return extension;
}

}  // namespace

void BrowserLiveTabContext::ShowBrowserWindow() {
  browser_->window()->Show();
}

SessionID BrowserLiveTabContext::GetSessionID() const {
  return browser_->session_id();
}

int BrowserLiveTabContext::GetTabCount() const {
  return browser_->tab_strip_model()->count();
}

int BrowserLiveTabContext::GetSelectedIndex() const {
  return browser_->tab_strip_model()->active_index();
}

std::string BrowserLiveTabContext::GetAppName() const {
  return browser_->app_name();
}

sessions::LiveTab* BrowserLiveTabContext::GetLiveTabAt(int index) const {
  return sessions::ContentLiveTab::GetForWebContents(
      browser_->tab_strip_model()->GetWebContentsAt(index));
}

sessions::LiveTab* BrowserLiveTabContext::GetActiveLiveTab() const {
  return sessions::ContentLiveTab::GetForWebContents(
      browser_->tab_strip_model()->GetActiveWebContents());
}

bool BrowserLiveTabContext::IsTabPinned(int index) const {
  return browser_->tab_strip_model()->IsTabPinned(index);
}

base::Optional<base::Token> BrowserLiveTabContext::GetTabGroupForTab(
    int index) const {
  const base::Optional<TabGroupId> group_id =
      browser_->tab_strip_model()->GetTabGroupForTab(index);
  return group_id.has_value() ? base::make_optional(group_id.value().token())
                              : base::nullopt;
}

BrowserLiveTabContext::TabGroupMetadata
BrowserLiveTabContext::GetTabGroupMetadata(base::Token group) const {
  const TabGroupVisualData* metadata =
      browser_->tab_strip_model()->GetVisualDataForGroup(
          TabGroupId::FromRawToken(group));
  DCHECK(metadata);
  return TabGroupMetadata{metadata->title(), metadata->color()};
}

const gfx::Rect BrowserLiveTabContext::GetRestoredBounds() const {
  return browser_->window()->GetRestoredBounds();
}

ui::WindowShowState BrowserLiveTabContext::GetRestoredState() const {
  return browser_->window()->GetRestoredState();
}

std::string BrowserLiveTabContext::GetWorkspace() const {
  return browser_->window()->GetWorkspace();
}

sessions::LiveTab* BrowserLiveTabContext::AddRestoredTab(
    const std::vector<sessions::SerializedNavigationEntry>& navigations,
    int tab_index,
    int selected_navigation,
    const std::string& extension_app_id,
    base::Optional<base::Token> group,
    bool select,
    bool pin,
    bool from_last_session,
    const sessions::PlatformSpecificTabData* tab_platform_data,
    const std::string& user_agent_override) {
  SessionStorageNamespace* storage_namespace =
      tab_platform_data
          ? static_cast<const sessions::ContentPlatformSpecificTabData*>(
                tab_platform_data)
                ->session_storage_namespace()
          : nullptr;

  WebContents* web_contents = chrome::AddRestoredTab(
      browser_, navigations, tab_index, selected_navigation, extension_app_id,
      group, select, pin, from_last_session, base::TimeTicks(),
      storage_namespace, user_agent_override, false /* from_session_restore */);

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  // The focused tab will be loaded by Browser, and TabLoader will load the
  // rest.
  if (!select) {
    // Regression check: make sure that the tab hasn't started to load
    // immediately.
    DCHECK(web_contents->GetController().NeedsReload());
    DCHECK(!web_contents->IsLoading());
  }
  std::vector<TabLoader::RestoredTab> restored_tabs;
  restored_tabs.emplace_back(web_contents, select, !extension_app_id.empty(),
                             pin, group);
  TabLoader::RestoreTabs(restored_tabs, base::TimeTicks::Now());
#else   // BUILDFLAG(ENABLE_SESSION_SERVICE)
  // Load the tab manually if there is no TabLoader.
  web_contents->GetController().LoadIfNecessary();
#endif  // BUILDFLAG(ENABLE_SESSION_SERVICE)

  return sessions::ContentLiveTab::GetForWebContents(web_contents);
}

sessions::LiveTab* BrowserLiveTabContext::ReplaceRestoredTab(
    const std::vector<sessions::SerializedNavigationEntry>& navigations,
    base::Optional<base::Token> group,
    int selected_navigation,
    bool from_last_session,
    const std::string& extension_app_id,
    const sessions::PlatformSpecificTabData* tab_platform_data,
    const std::string& user_agent_override) {
  SessionStorageNamespace* storage_namespace =
      tab_platform_data
          ? static_cast<const sessions::ContentPlatformSpecificTabData*>(
                tab_platform_data)
                ->session_storage_namespace()
          : nullptr;

  WebContents* web_contents = chrome::ReplaceRestoredTab(
      browser_, navigations, selected_navigation, from_last_session,
      extension_app_id, storage_namespace, user_agent_override,
      false /* from_session_restore */);

  return sessions::ContentLiveTab::GetForWebContents(web_contents);
}

void BrowserLiveTabContext::CloseTab() {
  chrome::CloseTab(browser_);
}

void BrowserLiveTabContext::SetTabGroupMetadata(
    base::Token group,
    TabGroupMetadata group_metadata) {
  TabGroupVisualData restored_data(std::move(group_metadata.title),
                                   group_metadata.color);
  browser_->tab_strip_model()->SetVisualDataForGroup(
      TabGroupId::FromRawToken(group), std::move(restored_data));
}

// static
sessions::LiveTabContext* BrowserLiveTabContext::Create(
    Profile* profile,
    const std::string& app_name,
    const gfx::Rect& bounds,
    ui::WindowShowState show_state,
    const std::string& workspace) {
  std::unique_ptr<Browser::CreateParams> create_params;
  if (ShouldCreateAppWindowForAppName(profile, app_name)) {
    // Only trusted app popup windows should ever be restored.
    create_params = std::make_unique<Browser::CreateParams>(
        Browser::CreateParams::CreateForApp(app_name, true /* trusted_source */,
                                            bounds, profile,
                                            true /* user_gesture */));
  } else {
    create_params = std::make_unique<Browser::CreateParams>(
        Browser::CreateParams(profile, true));
    create_params->initial_bounds = bounds;
  }

  create_params->initial_show_state = show_state;
  create_params->initial_workspace = workspace;
  Browser* browser = new Browser(*create_params.get());
  return browser->live_tab_context();
}

// static
sessions::LiveTabContext* BrowserLiveTabContext::FindContextForWebContents(
    const WebContents* contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  return browser ? browser->live_tab_context() : nullptr;
}

// static
sessions::LiveTabContext* BrowserLiveTabContext::FindContextWithID(
    SessionID desired_id) {
  Browser* browser = chrome::FindBrowserWithID(desired_id);
  return browser ? browser->live_tab_context() : nullptr;
}
