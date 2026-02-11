// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip_internals/tab_strip_internals_handler.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_service_base.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/webui/tab_strip_internals/tab_strip_internals_observer.h"
#include "chrome/browser/ui/webui/tab_strip_internals/tab_strip_internals_util.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/web_contents.h"

TabStripInternalsPageHandler::TabStripInternalsPageHandler(
    content::WebContents* web_contents,
    mojo::PendingReceiver<tab_strip_internals::mojom::PageHandler> receiver,
    mojo::PendingRemote<tab_strip_internals::mojom::Page> page)
    : content::WebContentsObserver(web_contents),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())) {
  observer_ = std::make_unique<TabStripInternalsObserver>(
      profile_,
      base::BindRepeating(&TabStripInternalsPageHandler::NotifyTabStripUpdated,
                          weak_ptr_factory_.GetWeakPtr()));
}

TabStripInternalsPageHandler::~TabStripInternalsPageHandler() = default;

void TabStripInternalsPageHandler::GetTabStripData(
    GetTabStripDataCallback callback) {
  // Only one in-flight request is supported at a time.
  if (pending_callback_) {
    std::move(callback).Run(BuildSnapshot());
    return;
  }

  pending_callback_ = std::move(callback);

  // This feature does not support fetching cross-profile session restore
  // metadata. Hence, session restore metadata is fetched only for the given
  // profile instance.
  if (!profile_->IsOffTheRecord()) {
    SessionServiceBase* session_service =
        SessionServiceFactory::GetForProfileForSessionRestore(profile_);
    if (session_service) {
      session_service->GetLastSession(
          base::BindOnce(&TabStripInternalsPageHandler::OnGotSavedSession,
                         weak_ptr_factory_.GetWeakPtr()));
      return;  // Wait for async callback.
    }
  }

  std::move(pending_callback_).Run(BuildSnapshot());
}

tab_strip_internals::mojom::ContainerPtr
TabStripInternalsPageHandler::BuildSnapshot() {
  auto data = tab_strip_internals::mojom::Container::New();
  data->tabstrip_tree = tab_strip_internals::mojom::TabStripTree::New();
  data->tab_restore = tab_strip_internals::mojom::TabRestoreData::New();
  data->restored_session =
      tab_strip_internals::mojom::SessionRestoreData::New();
  data->saved_session = tab_strip_internals::mojom::SessionRestoreData::New();

  for (const auto* browser : GetAllBrowserWindowInterfaces()) {
    auto window_node = tab_strip_internals::mojom::WindowNode::New();

    window_node->id = tab_strip_internals::MakeNodeId(
        base::NumberToString(browser->GetSessionID().id()),
        tab_strip_internals::mojom::NodeId::Type::kWindow);

    window_node->tabstrip_model =
        tab_strip_internals::mojom::TabStripModel::New(
            tab_strip_internals::BuildTabCollectionTree(
                browser->GetTabStripModel()));

    window_node->selection_model =
        tab_strip_internals::BuildSelectionModel(browser->GetTabStripModel());

    data->tabstrip_tree->windows.push_back(std::move(window_node));
  }

  // This feature does not support fetching cross-profile tab restore metadata.
  // Hence, TabRestore metadata is fetched only for the given profile instance.
  if (!profile_->IsOffTheRecord()) {
    sessions::TabRestoreService* tab_restore_service =
        TabRestoreServiceFactory::GetForProfile(profile_);
    if (tab_restore_service) {
      data->tab_restore = tab_strip_internals::BuildTabRestoreData(
          tab_restore_service->entries());
    }

    if (!saved_session_windows_.empty()) {
      data->saved_session =
          tab_strip_internals::BuildSessionRestoreData(saved_session_windows_);
    }
    data->restored_session = tab_strip_internals::BuildSessionRestoreData(
        observer_->GetRestoredSession());
  }

  return data;
}

void TabStripInternalsPageHandler::NotifyTabStripUpdated() {
  if (page_ && web_contents() &&
      web_contents()->GetVisibility() == content::Visibility::VISIBLE) {
    page_->OnTabStripUpdated(BuildSnapshot());
  }
}

void TabStripInternalsPageHandler::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::VISIBLE) {
    NotifyTabStripUpdated();
  }
}

void TabStripInternalsPageHandler::OnGotSavedSession(
    std::vector<std::unique_ptr<sessions::SessionWindow>> windows,
    SessionID /*active_window_id*/,
    bool read_error) {
  if (!read_error) {
    saved_session_windows_ = std::move(windows);
  }

  auto snapshot = BuildSnapshot();
  std::move(pending_callback_).Run(std::move(snapshot));
}
