// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CONTENT_CONTENT_LIVE_TAB_H_
#define COMPONENTS_SESSIONS_CONTENT_CONTENT_LIVE_TAB_H_

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "components/sessions/content/content_serialized_navigation_builder.h"
#include "components/sessions/core/live_tab.h"
#include "components/sessions/core/sessions_export.h"
#include "content/public/browser/web_contents.h"

namespace content {
class NavigationController;
}

class TabRestoreServiceImplTest;

namespace sessions {

// An implementation of LiveTab that is backed by content::WebContents for use
// on //content-based platforms.
class SESSIONS_EXPORT ContentLiveTab
    : public LiveTab,
      public base::SupportsUserData::Data {
 public:
  ContentLiveTab(const ContentLiveTab&) = delete;
  ContentLiveTab& operator=(const ContentLiveTab&) = delete;

  ~ContentLiveTab() override;

  // Returns the ContentLiveTab associated with |web_contents|, creating it if
  // it has not already been created.
  static ContentLiveTab* GetForWebContents(content::WebContents* web_contents);

  // LiveTab:
  bool IsInitialBlankNavigation() override;
  int GetCurrentEntryIndex() override;
  int GetPendingEntryIndex() override;
  sessions::SerializedNavigationEntry GetEntryAtIndex(int index) override;
  sessions::SerializedNavigationEntry GetPendingEntry() override;
  int GetEntryCount() override;
  std::unique_ptr<tab_restore::PlatformSpecificTabData>
  GetPlatformSpecificTabData() override;
  SerializedUserAgentOverride GetUserAgentOverride() override;

  content::WebContents* web_contents() { return web_contents_; }
  const content::WebContents* web_contents() const { return web_contents_; }

 private:
  friend class base::SupportsUserData;
  friend class ::TabRestoreServiceImplTest;

  explicit ContentLiveTab(content::WebContents* contents);

  content::NavigationController& navigation_controller() {
    return web_contents_->GetController();
  }

  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CONTENT_CONTENT_LIVE_TAB_H_
