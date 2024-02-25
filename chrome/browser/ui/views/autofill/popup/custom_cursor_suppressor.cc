// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/custom_cursor_suppressor.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_registry.h"

CustomCursorSuppressor::NavigationObserver::NavigationObserver(
    content::WebContents* web_contents,
    Callback callback)
    : content::WebContentsObserver(web_contents),
      callback_(std::move(callback)) {
  CHECK(callback_);
}

CustomCursorSuppressor::NavigationObserver::~NavigationObserver() = default;

void CustomCursorSuppressor::NavigationObserver::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  if (new_host && new_host->IsInPrimaryMainFrame()) {
    callback_.Run(*web_contents());
  }
}

CustomCursorSuppressor::CustomCursorSuppressor() = default;

CustomCursorSuppressor::~CustomCursorSuppressor() = default;

void CustomCursorSuppressor::Start(int max_dimension_dips) {
  max_dimension_dips_ = max_dimension_dips;

  // Observe the list of browsers.
  browser_list_observation_.Observe(BrowserList::GetInstance());
  // Observe all TabStripModels of existing browsers and suppress custom cursors
  // on their active tabs.
  for (Browser* browser : *BrowserList::GetInstance()) {
    browser->tab_strip_model()->AddObserver(this);
    if (content::WebContents* active_contents =
            browser->tab_strip_model()->GetActiveWebContents()) {
      MaybeObserveNavigationsInWebContents(*active_contents);
      SuppressForWebContents(*active_contents);
    }
    ObserveAndSuppressExtensionsForProfile(*browser->profile());
  }
}

void CustomCursorSuppressor::Stop() {
  disallow_custom_cursor_scopes_.clear();
  TabStripModelObserver::StopObservingAll(this);
  browser_list_observation_.Reset();
  extension_host_registry_observation_.RemoveAllObservations();
}

bool CustomCursorSuppressor::IsSuppressing(
    content::WebContents& web_contents) const {
  return disallow_custom_cursor_scopes_.contains(
      web_contents.GetPrimaryMainFrame()->GetGlobalId());
}

std::vector<content::GlobalRenderFrameHostId>
CustomCursorSuppressor::SuppressedRenderFrameHostIdsForTesting() const {
  std::vector<content::GlobalRenderFrameHostId> rfh_ids;
  for (const auto& [rfh_id, scope] : disallow_custom_cursor_scopes_) {
    rfh_ids.push_back(rfh_id);
  }
  return rfh_ids;
}

void CustomCursorSuppressor::ObserveAndSuppressExtensionsForProfile(
    Profile& profile) {
  auto* const registry = extensions::ExtensionHostRegistry::Get(&profile);
  if (!registry ||
      extension_host_registry_observation_.IsObservingSource(registry)) {
    return;
  }
  extension_host_registry_observation_.AddObservation(registry);
  // Suppress custom cursors on all existing extension hosts.
  for (extensions::ExtensionHost* host : registry->extension_hosts()) {
    if (host->document_element_available()) {
      MaybeObserveNavigationsInWebContents(*host->host_contents());
      SuppressForWebContents(*host->host_contents());
    }
  }
}

void CustomCursorSuppressor::SuppressForWebContents(
    content::WebContents& web_contents) {
  if (IsSuppressing(web_contents)) {
    return;
  }
  disallow_custom_cursor_scopes_.insert(
      {web_contents.GetPrimaryMainFrame()->GetGlobalId(),
       web_contents.CreateDisallowCustomCursorScope(max_dimension_dips_)});
}

void CustomCursorSuppressor::MaybeObserveNavigationsInWebContents(
    content::WebContents& web_contents) {
  if (IsSuppressing(web_contents)) {
    return;
  }
  navigation_observers_.push_back(std::make_unique<NavigationObserver>(
      &web_contents,
      base::BindRepeating(&CustomCursorSuppressor::SuppressForWebContents,
                          base::Unretained(this))));
}

void CustomCursorSuppressor::OnBrowserAdded(Browser* browser) {
  browser->tab_strip_model()->AddObserver(this);
  ObserveAndSuppressExtensionsForProfile(*browser->profile());
}

void CustomCursorSuppressor::OnExtensionHostRegistryShutdown(
    extensions::ExtensionHostRegistry* registry) {
  CHECK(extension_host_registry_observation_.IsObservingSource(registry));
  extension_host_registry_observation_.RemoveObservation(registry);
}

void CustomCursorSuppressor::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed() && selection.new_contents) {
    MaybeObserveNavigationsInWebContents(*selection.new_contents);
    SuppressForWebContents(*selection.new_contents);
  }
}

void CustomCursorSuppressor::OnExtensionHostDocumentElementAvailable(
    content::BrowserContext* browser_context,
    extensions::ExtensionHost* extension_host) {
  CHECK(extension_host);
  CHECK(extension_host->host_contents());
  SuppressForWebContents(*extension_host->host_contents());
}
