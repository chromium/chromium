// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_CONTENT_CONTENT_INFOBAR_MANAGER_H_
#define COMPONENTS_INFOBARS_CONTENT_CONTENT_INFOBAR_MANAGER_H_

#include "build/build_config.h"
#include "components/infobars/core/infobar_manager.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/base/window_open_disposition.h"

namespace content {
struct LoadCommittedDetails;
class WebContents;
}  // namespace content

namespace infobars {

class InfoBar;

// Associates a WebContents to an InfoBarManager.
// It manages the infobar notifications and responds to navigation events.
class ContentInfoBarManager
    : public InfoBarManager,
      public content::WebContentsObserver,
      public content::WebContentsUserData<ContentInfoBarManager> {
 public:
  explicit ContentInfoBarManager(content::WebContents* web_contents);

  ContentInfoBarManager(const ContentInfoBarManager&) = delete;
  ContentInfoBarManager& operator=(const ContentInfoBarManager&) = delete;

  ~ContentInfoBarManager() override;

  static InfoBarDelegate::NavigationDetails
  NavigationDetailsFromLoadCommittedDetails(
      const content::LoadCommittedDetails& details);

  // This function must only be called on infobars that are owned by a
  // ContentInfoBarManager instance (or not owned at all, in which case this
  // returns nullptr).
  static content::WebContents* WebContentsFromInfoBar(InfoBar* infobar);

  // Makes it so the next reload is ignored. That is, if the next commit is a
  // reload then it is treated as if nothing happened and no infobars are
  // attempted to be closed.
  // This is useful for non-user triggered reloads that should not dismiss
  // infobars. For example, instant may trigger a reload when the google URL
  // changes.
  void set_ignore_next_reload() { ignore_next_reload_ = true; }

  // InfoBarManager:
  void OpenURL(const GURL& url, WindowOpenDisposition disposition) override;

 private:
  friend class content::WebContentsUserData<ContentInfoBarManager>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  // InfoBarManager:
  int GetActiveEntryID() override;

  // content::WebContentsObserver:
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
  void WebContentsDestroyed() override;

  // See description in set_ignore_next_reload().
  bool ignore_next_reload_ = false;
};

}  // namespace infobars

#endif  // COMPONENTS_INFOBARS_CONTENT_CONTENT_INFOBAR_MANAGER_H_
