// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/contextual_search/desktop_query_contextualizer_delegate.h"

#include "base/functional/bind.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "components/contextual_tasks/public/features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_features.h"
#else
#include "chrome/browser/ui/tabs/public/tab_features.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace contextual_tasks {

DesktopQueryContextualizerDelegate::DesktopQueryContextualizerDelegate(
    GetSessionHandleCallback get_session_callback,
    GetViewportEncodingOptionsCallback get_viewport_options_callback,
    ContextualTasksContextService* service,
    BrowserWindowInterface* browser_window_interface)
    : get_session_callback_(std::move(get_session_callback)),
      get_viewport_options_callback_(std::move(get_viewport_options_callback)),
      service_(service),
      browser_window_interface_(browser_window_interface
                                    ? browser_window_interface->GetWeakPtr()
                                    : nullptr) {}

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

void DesktopQueryContextualizerDelegate::GetRelevantTabsForQuery(
    const std::string& query_text,
    const std::vector<GURL>& attached_context_urls,
    base::OnceCallback<void(std::vector<QueryContextualizer::TabId>)>
        callback) {
  if (!service_ || !browser_window_interface_) {
    std::move(callback).Run({});
    return;
  }

  contextual_tasks::TabSelectionOptions tab_selection_options;
  tab_selection_options.tab_selection_timeout =
      contextual_tasks::GetSmartTabSharingTabSelectionTimeout();
  tab_selection_options.browser_window_interface = browser_window_interface_;

  service_->GetRelevantTabsForQuery(
      tab_selection_options, query_text, attached_context_urls,
      base::BindOnce(
          [](base::OnceCallback<void(std::vector<QueryContextualizer::TabId>)>
                 cb,
             std::vector<base::WeakPtr<content::WebContents>> relevant_tabs) {
            std::vector<QueryContextualizer::TabId> tab_ids;
            for (const auto& weak_wc : relevant_tabs) {
              if (weak_wc) {
                tabs::TabInterface* tab =
                    tabs::TabInterface::MaybeGetFromContents(weak_wc.get());
                if (tab) {
                  tab_ids.push_back(tab->GetHandle().raw_value());
                }
              }
            }
            std::move(cb).Run(std::move(tab_ids));
          },
          std::move(callback)));
}

tabs::TabInterface* DesktopQueryContextualizerDelegate::GetTab(
    QueryContextualizer::TabId id) {
  tabs::TabHandle handle = tabs::TabHandle(id);
  return handle.Get();
}

}  // namespace contextual_tasks
