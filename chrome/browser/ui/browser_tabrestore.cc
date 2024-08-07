// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tabrestore.h"

#include <map>
#include <memory>
#include <utility>

#include "build/build_config.h"
#include "chrome/browser/apps/app_service/web_contents_app_id_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_service_base.h"
#include "chrome/browser/sessions/session_service_lookup.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/restore_type.h"
#include "content/public/browser/session_storage_namespace.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/range/range.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/side_search/side_search_utils.h"
#endif  // defined(TOOLKIT_VIEWS)

using content::NavigationEntry;
using content::RestoreType;
using content::WebContents;
using sessions::ContentSerializedNavigationBuilder;
using sessions::SerializedNavigationEntry;

namespace chrome {

namespace {

std::unique_ptr<WebContents> CreateRestoredTab(
    Browser* browser,
    const std::vector<SerializedNavigationEntry>& navigations,
    int selected_navigation,
    const std::string& extension_app_id,
    base::TimeTicks last_active_time_ticks,
    base::Time last_active_time,
    content::SessionStorageNamespace* session_storage_namespace,
    const sessions::SerializedUserAgentOverride& user_agent_override,
    const std::map<std::string, std::string>& extra_data,
    bool initially_hidden,
    bool from_session_restore) {
  GURL restore_url = navigations.at(selected_navigation).virtual_url();
  // TODO(ajwong): Remove the temporary session_storage_namespace_map when
  // we teach session restore to understand that one tab can have multiple
  // SessionStorageNamespace objects. Also remove the
  // session_storage_namespace.h include since we only need that to assign
  // into the map.
  content::SessionStorageNamespaceMap session_storage_namespace_map =
      content::CreateMapWithDefaultSessionStorageNamespace(
          browser->profile(), session_storage_namespace);
  WebContents::CreateParams create_params(
      browser->profile(),
      tab_util::GetSiteInstanceForNewTab(browser->profile(), restore_url));
  create_params.initially_hidden = initially_hidden;
  create_params.desired_renderer_state =
      WebContents::CreateParams::kNoRendererProcess;
  create_params.last_active_time_ticks = last_active_time_ticks;
  create_params.last_active_time = last_active_time;
  std::unique_ptr<WebContents> web_contents =
      WebContents::CreateWithSessionStorage(create_params,
                                            session_storage_namespace_map);
  if (from_session_restore) {
    SessionRestore::OnWillRestoreTab(web_contents.get());
  }
  apps::SetAppIdForWebContents(browser->profile(), web_contents.get(),
                               extension_app_id);

  std::vector<std::unique_ptr<NavigationEntry>> entries =
      ContentSerializedNavigationBuilder::ToNavigationEntries(
          navigations, browser->profile());

  blink::UserAgentOverride ua_override;
  ua_override.ua_string_override = user_agent_override.ua_string_override;
  ua_override.ua_metadata_override = blink::UserAgentMetadata::Demarshal(
      user_agent_override.opaque_ua_metadata_override);
  web_contents->SetUserAgentOverride(ua_override, false);
  web_contents->GetController().Restore(selected_navigation,
                                        RestoreType::kRestored, &entries);
  DCHECK_EQ(0u, entries.size());

#if defined(TOOLKIT_VIEWS)
  if (IsSideSearchEnabled(browser->profile())) {
    side_search::SetSideSearchTabStateFromRestoreData(web_contents.get(),
                                                      extra_data);
  }
#endif  // defined(TOOLKIT_VIEWS)

  return web_contents;
}

// Start loading a restored tab after adding it to its browser, if visible.
//
// Without this, loading starts when
// WebContentsImpl::UpdateWebContentsVisibility(VISIBLE) is invoked, which
// happens at a different time on Mac vs. other desktop platform due to a
// different windowing system. Starting to load here ensures consistent behavior
// across desktop platforms and allows FirstWebContentsProfiler to have strict
// cross-platform expectations about events it observes.
void LoadRestoredTabIfVisible(Browser* browser,
                              content::WebContents* web_contents) {
  if (web_contents->GetVisibility() != content::Visibility::VISIBLE) {
    return;
  }

  DCHECK_EQ(browser->tab_strip_model()->GetActiveWebContents(), web_contents);
  // A layout should already have been performed to determine the contents size.
  // The contents size should not be empty, unless the browser size and restored
  // size are also empty.
  DCHECK(!browser->window()->GetContentsSize().IsEmpty() ||
         (browser->window()->GetBounds().IsEmpty() &&
          browser->window()->GetRestoredBounds().IsEmpty()));
  DCHECK_EQ(web_contents->GetSize(), browser->window()->GetContentsSize());

  web_contents->GetController().LoadIfNecessary();
}

WebContents* AddRestoredTabImpl(std::unique_ptr<WebContents> web_contents,
                                Browser* browser,
                                int tab_index,
                                std::optional<tab_groups::TabGroupId> group,
                                bool select,
                                bool pin,
                                bool from_session_restore,
                                std::optional<bool> is_active_browser) {
  TabStripModel* const tab_strip_model = browser->tab_strip_model();

  int add_types = select ? AddTabTypes::ADD_ACTIVE : AddTabTypes::ADD_NONE;
  if (pin) {
    tab_index =
        std::min(tab_index, tab_strip_model->IndexOfFirstNonPinnedTab());
    add_types |= AddTabTypes::ADD_PINNED;
  }

  if (tab_strip_model->group_model()) {
    const std::optional<tab_groups::TabGroupId> surrounding_group =
        tab_strip_model->GetSurroundingTabGroup(tab_index);

    // If inserting at |tab_index| would put the tab within a different
    // group, adjust the index to put it outside.
    if (surrounding_group && surrounding_group != group) {
      tab_index = tab_strip_model->group_model()
                      ->GetTabGroup(*surrounding_group)
                      ->ListTabs()
                      .end();
    }

    // `tab_index` should respect group contiguity.
    if (group.has_value() &&
        tab_strip_model->group_model()->ContainsTabGroup(group.value())) {
      gfx::Range group_indices = tab_strip_model->group_model()
                                     ->GetTabGroup(group.value())
                                     ->ListTabs();
      tab_index = std::clamp(tab_index, static_cast<int>(group_indices.start()),
                             static_cast<int>(group_indices.end()));
    }
  }

  WebContents* raw_web_contents = web_contents.get();

  // The two cases we could run into are -
  // 1. Tab was a part of a group that is no longer present.
  // 2. Tab is added to a group that is present in the tabstrip model or is an
  // ungrouped tab.
  if (group.has_value() && tab_strip_model->group_model() &&
      !tab_strip_model->group_model()->ContainsTabGroup(group.value())) {
    // Insert as a ungrouped tab and then add it to the new group.
    const int actual_index = tab_strip_model->InsertWebContentsAt(
        tab_index, std::move(web_contents), add_types);
    tab_strip_model->AddToGroupForRestore({actual_index}, group.value());
  } else {
    tab_strip_model->InsertWebContentsAt(tab_index, std::move(web_contents),
                                         add_types, group);
  }

  // We set the size of the view here, before Blink does its initial layout.
  // If we don't, the initial layout of background tabs will be performed
  // with a view width of 0, which may cause script outputs and anchor link
  // location calculations to be incorrect even after a new layout with
  // proper view dimensions. TabStripModel::AddWebContents() contains similar
  // logic.
  //
  // TODO(crbug.com/40113932): There should be a way to ask the browser
  // to perform a layout so that size of the WebContents is right.
  gfx::Size size = browser->window()->GetContentsSize();
  // Fallback to the restore bounds if it's empty as the window is not shown
  // yet and the bounds may not be available on all platforms.
  if (size.IsEmpty()) {
    size = browser->window()->GetRestoredBounds().size();
  }
  raw_web_contents->Resize(gfx::Rect(size));

  const bool initially_hidden = !select || browser->window()->IsMinimized();
  if (initially_hidden) {
    raw_web_contents->WasHidden();
  } else {
    const bool should_activate =
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
        // Activating a window on another space causes the system to switch to
        // that space. Since the session restore process shows and activates
        // windows itself, activating windows here should be safe to skip.
        // Cautiously apply only to Windows and MacOS, for now
        // (https://crbug.com/1019048).
        !from_session_restore;
#else
        true;
#endif
    if (should_activate) {
      browser->window()->Activate();
    }
  }

  SessionServiceBase* session_service =
      GetAppropriateSessionServiceIfExisting(browser);
  if (session_service) {
    session_service->TabRestored(raw_web_contents, pin);
  }

  // Immediate load if the browser activeness is true or unknown. That is, do
  // not do immediate load for browsers that are known to be inactive.
  bool should_load = is_active_browser.value_or(true);

// On OS_MAC, `LoadRestoredTabIfVisible` by default so that its tab loading
// behaves like other platforms to make FirstWebContentsProfiler wor
// properly. However, app restorations take longer than the normal browser
// window to be restored and that will cause `LoadRestoredTabIfVisible()` to
// fail. Skip LoadRestoredTabIfVisible if OS_MAC && the browser is an app
// browser.
#if BUILDFLAG(IS_MAC)
  should_load = (browser->type() != Browser::Type::TYPE_APP);
#endif  // BUILDFLAG(IS_MAC)

  if (should_load) {
    LoadRestoredTabIfVisible(browser, raw_web_contents);
  }

  return raw_web_contents;
}

}  // namespace

WebContents* AddRestoredTab(
    Browser* browser,
    const std::vector<SerializedNavigationEntry>& navigations,
    int tab_index,
    int selected_navigation,
    const std::string& extension_app_id,
    std::optional<tab_groups::TabGroupId> group,
    bool select,
    bool pin,
    base::TimeTicks last_active_time_ticks,
    base::Time last_active_time,
    content::SessionStorageNamespace* session_storage_namespace,
    const sessions::SerializedUserAgentOverride& user_agent_override,
    const std::map<std::string, std::string>& extra_data,
    bool from_session_restore,
    std::optional<bool> is_active_browser) {
  const bool initially_hidden = !select || browser->window()->IsMinimized();
  std::unique_ptr<WebContents> web_contents = CreateRestoredTab(
      browser, navigations, selected_navigation, extension_app_id,
      last_active_time_ticks, last_active_time, session_storage_namespace,
      user_agent_override, extra_data, initially_hidden, from_session_restore);

  return AddRestoredTabImpl(std::move(web_contents), browser, tab_index, group,
                            select, pin, from_session_restore,
                            is_active_browser);
}

WebContents* ReplaceRestoredTab(
    Browser* browser,
    const std::vector<SerializedNavigationEntry>& navigations,
    int selected_navigation,
    const std::string& extension_app_id,
    content::SessionStorageNamespace* session_storage_namespace,
    const sessions::SerializedUserAgentOverride& user_agent_override,
    const std::map<std::string, std::string>& extra_data,
    bool from_session_restore) {
  std::unique_ptr<WebContents> web_contents = CreateRestoredTab(
      browser, navigations, selected_navigation, extension_app_id,
      base::TimeTicks(), base::Time(), session_storage_namespace,
      user_agent_override, extra_data, false, from_session_restore);
  WebContents* raw_web_contents = web_contents.get();

  // ReplaceWebContentsAt won't animate in the restoration, so manually do the
  // equivalent of ReplaceWebContentsAt.
  TabStripModel* tab_strip = browser->tab_strip_model();
  int insertion_index = tab_strip->active_index();
  tab_strip->InsertWebContentsAt(
      insertion_index + 1, std::move(web_contents),
      AddTabTypes::ADD_ACTIVE | AddTabTypes::ADD_INHERIT_OPENER);
  tab_strip->CloseWebContentsAt(insertion_index, TabCloseTypes::CLOSE_NONE);

  LoadRestoredTabIfVisible(browser, raw_web_contents);

  return raw_web_contents;
}

}  // namespace chrome
