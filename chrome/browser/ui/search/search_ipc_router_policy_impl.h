// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_SEARCH_IPC_ROUTER_POLICY_IMPL_H_
#define CHROME_BROWSER_UI_SEARCH_SEARCH_IPC_ROUTER_POLICY_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/search/search_ipc_router.h"

#if BUILDFLAG(IS_ANDROID)
#error "Instant is only used on desktop";
#endif

namespace content {
class WebContents;
}

// The SearchIPCRouter::Policy implementation.
class SearchIPCRouterPolicyImpl : public SearchIPCRouter::Policy {
 public:
  explicit SearchIPCRouterPolicyImpl(content::WebContents* web_contents);

  SearchIPCRouterPolicyImpl(const SearchIPCRouterPolicyImpl&) = delete;
  SearchIPCRouterPolicyImpl& operator=(const SearchIPCRouterPolicyImpl&) =
      delete;

  ~SearchIPCRouterPolicyImpl() override;

 private:
  friend class SearchIPCRouterPolicyTest;

  // Overridden from SearchIPCRouter::Policy:
  bool ShouldProcessFocusOmnibox(bool is_active_tab) override;
  bool ShouldProcessDeleteMostVisitedItem() override;
  bool ShouldProcessUndoMostVisitedDeletion() override;
  bool ShouldProcessUndoAllMostVisitedDeletions() override;
  bool ShouldSendSetInputInProgress(bool is_active_tab) override;
  bool ShouldSendOmniboxFocusChanged() override;
  bool ShouldSendMostVisitedInfo() override;
  bool ShouldSendNtpTheme() override;
  bool ShouldProcessThemeChangeMessages() override;

  // Used by unit tests.
  void set_is_incognito(bool is_incognito) {
    is_incognito_ = is_incognito;
  }

  raw_ptr<content::WebContents> web_contents_;
  bool is_incognito_;
};

#endif  // CHROME_BROWSER_UI_SEARCH_SEARCH_IPC_ROUTER_POLICY_IMPL_H_
