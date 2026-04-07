// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/contextual_search/desktop_query_contextualizer_delegate.h"

#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace contextual_tasks {

DesktopQueryContextualizerDelegate::DesktopQueryContextualizerDelegate(
    GetSessionHandleCallback get_session_callback,
    GetViewportEncodingOptionsCallback get_viewport_options_callback)
    : get_session_callback_(std::move(get_session_callback)),
      get_viewport_options_callback_(std::move(get_viewport_options_callback)) {
}

DesktopQueryContextualizerDelegate::~DesktopQueryContextualizerDelegate() =
    default;

GURL DesktopQueryContextualizerDelegate::GetTabUrl(
    QueryContextualizer::TabId id) {
  auto* tab = GetTab(id);
  if (!tab || !tab->GetContents()) {
    return GURL();
  }
  return tab->GetContents()->GetLastCommittedURL();
}

SessionID DesktopQueryContextualizerDelegate::GetTabSessionId(
    QueryContextualizer::TabId id) {
  auto* tab = GetTab(id);
  if (!tab || !tab->GetContents()) {
    return SessionID::InvalidValue();
  }
  return sessions::SessionTabHelper::IdForTab(tab->GetContents());
}

void DesktopQueryContextualizerDelegate::GetPageContext(
    QueryContextualizer::TabId id,
    base::OnceCallback<void(std::unique_ptr<lens::ContextualInputData>)>
        callback) {
  auto* tab = GetTab(id);
  if (!tab) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto* tab_features = tab->GetTabFeatures();
  if (!tab_features || !tab_features->tab_contextualization_controller()) {
    std::move(callback).Run(nullptr);
    return;
  }

  tab_features->tab_contextualization_controller()->GetPageContext(
      std::move(callback));
}

bool DesktopQueryContextualizerDelegate::IsTabValid(
    QueryContextualizer::TabId id) {
  return GetTab(id) != nullptr;
}

std::optional<lens::ImageEncodingOptions> DesktopQueryContextualizerDelegate::
    GetTabViewportEncodingOptionsForQueryContextualizer() {
  return get_viewport_options_callback_.Run();
}

contextual_search::ContextualSearchSessionHandle*
DesktopQueryContextualizerDelegate::
    GetOrCreateSessionHandleForQueryContextualizer() {
  return get_session_callback_.Run();
}

tabs::TabInterface* DesktopQueryContextualizerDelegate::GetTab(
    QueryContextualizer::TabId id) {
  tabs::TabHandle handle = tabs::TabHandle(id);
  return handle.Get();
}

}  // namespace contextual_tasks
