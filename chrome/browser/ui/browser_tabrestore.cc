// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tabrestore.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_group_id.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_contents_sizer.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/web_contents.h"

using content::NavigationEntry;
using content::RestoreType;
using content::WebContents;
using sessions::ContentSerializedNavigationBuilder;
using sessions::SerializedNavigationEntry;

namespace chrome {

namespace {

RestoreType GetRestoreType(Browser* browser, bool from_last_session) {
  if (!from_last_session)
    return RestoreType::CURRENT_SESSION;
  return browser->profile()->GetLastSessionExitType() == Profile::EXIT_CRASHED
             ? RestoreType::LAST_SESSION_CRASHED
             : RestoreType::LAST_SESSION_EXITED_CLEANLY;
}

std::unique_ptr<WebContents> CreateRestoredTab(
    Browser* browser,
    const std::vector<SerializedNavigationEntry>& navigations,
    int selected_navigation,
    const std::string& extension_app_id,
    bool from_last_session,
    base::TimeTicks last_active_time,
    content::SessionStorageNamespace* session_storage_namespace,
    const std::string& user_agent_override,
    bool initially_hidden,
    bool from_session_restore) {
  GURL restore_url = navigations.at(selected_navigation).virtual_url();
  // TODO(ajwong): Remove the temporary session_storage_namespace_map when
  // we teach session restore to understand that one tab can have multiple
  // SessionStorageNamespace objects. Also remove the
  // session_storage_namespace.h include since we only need that to assign
  // into the map.
  content::SessionStorageNamespaceMap session_storage_namespace_map;
  session_storage_namespace_map[std::string()] = session_storage_namespace;
  WebContents::CreateParams create_params(
      browser->profile(),
      tab_util::GetSiteInstanceForNewTab(browser->profile(), restore_url));
  create_params.initially_hidden = initially_hidden;
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  create_params.last_active_time = last_active_time;
  std::unique_ptr<WebContents> web_contents =
      WebContents::CreateWithSessionStorage(create_params,
                                            session_storage_namespace_map);
  if (from_session_restore)
    SessionRestore::OnWillRestoreTab(web_contents.get());
  extensions::TabHelper::CreateForWebContents(web_contents.get());
  extensions::TabHelper::FromWebContents(web_contents.get())
      ->SetExtensionAppById(extension_app_id);
  std::vector<std::unique_ptr<NavigationEntry>> entries =
      ContentSerializedNavigationBuilder::ToNavigationEntries(
          navigations, browser->profile());
  web_contents->SetUserAgentOverride(user_agent_override, false);
  web_contents->GetController().Restore(
      selected_navigation, GetRestoreType(browser, from_last_session),
      &entries);
  DCHECK_EQ(0u, entries.size());

  return web_contents;
}

}  // namespace

WebContents* AddRestoredTab(
    Browser* browser,
    const std::vector<SerializedNavigationEntry>& navigations,
    int tab_index,
    int selected_navigation,
    const std::string& extension_app_id,
    base::Optional<base::Token> raw_group_id,
    bool select,
    bool pin,
    bool from_last_session,
    base::TimeTicks last_active_time,
    content::SessionStorageNamespace* session_storage_namespace,
    const std::string& user_agent_override,
    bool from_session_restore) {
  std::unique_ptr<WebContents> web_contents = CreateRestoredTab(
      browser, navigations, selected_navigation, extension_app_id,
      from_last_session, last_active_time, session_storage_namespace,
      user_agent_override, !select, from_session_restore);

  int add_types = select ? TabStripModel::ADD_ACTIVE : TabStripModel::ADD_NONE;
  if (pin) {
    tab_index = std::min(
        tab_index, browser->tab_strip_model()->IndexOfFirstNonPinnedTab());
    add_types |= TabStripModel::ADD_PINNED;
  }

  WebContents* raw_web_contents = web_contents.get();
  const int actual_index = browser->tab_strip_model()->InsertWebContentsAt(
      tab_index, std::move(web_contents), add_types);

  if (raw_group_id.has_value()) {
    auto group_id = TabGroupId::FromRawToken(raw_group_id.value());
    browser->tab_strip_model()->AddToGroupForRestore({actual_index}, group_id);
  }

  if (select) {
    if (
#if defined(OS_MACOSX)
        // Activating a window on another space causes the system to switch to
        // that space. Since the session restore process shows and activates
        // windows itself, activating windows here should be safe to skip.
        // Cautiously apply only to macOS, for now (https://crbug.com/1019048).
        !from_session_restore &&
#endif
        !browser->window()->IsMinimized())
      browser->window()->Activate();
  } else {
    // We set the size of the view here, before Blink does its initial layout.
    // If we don't, the initial layout of background tabs will be performed
    // with a view width of 0, which may cause script outputs and anchor link
    // location calculations to be incorrect even after a new layout with
    // proper view dimensions. TabStripModel::AddWebContents() contains similar
    // logic.
    gfx::Size size = browser->window()->GetContentsSize();
    // Fallback to the restore bounds if it's empty as the window is not shown
    // yet and the bounds may not be available on all platforms.
    if (size.IsEmpty())
      size = browser->window()->GetRestoredBounds().size();
    ResizeWebContents(raw_web_contents, gfx::Rect(size));
    raw_web_contents->WasHidden();
  }
  SessionService* session_service =
      SessionServiceFactory::GetForProfileIfExisting(browser->profile());
  if (session_service)
    session_service->TabRestored(raw_web_contents, pin);
  return raw_web_contents;
}

WebContents* ReplaceRestoredTab(
    Browser* browser,
    const std::vector<SerializedNavigationEntry>& navigations,
    int selected_navigation,
    bool from_last_session,
    const std::string& extension_app_id,
    content::SessionStorageNamespace* session_storage_namespace,
    const std::string& user_agent_override,
    bool from_session_restore) {
  std::unique_ptr<WebContents> web_contents = CreateRestoredTab(
      browser, navigations, selected_navigation, extension_app_id,
      from_last_session, base::TimeTicks(), session_storage_namespace,
      user_agent_override, false, from_session_restore);
  WebContents* raw_web_contents = web_contents.get();

  // ReplaceWebContentsAt won't animate in the restoration, so manually do the
  // equivalent of ReplaceWebContentsAt.
  TabStripModel* tab_strip = browser->tab_strip_model();
  int insertion_index = tab_strip->active_index();
  tab_strip->InsertWebContentsAt(
      insertion_index + 1, std::move(web_contents),
      TabStripModel::ADD_ACTIVE | TabStripModel::ADD_INHERIT_OPENER);
  tab_strip->CloseWebContentsAt(insertion_index, TabStripModel::CLOSE_NONE);
  return raw_web_contents;
}

}  // namespace chrome
