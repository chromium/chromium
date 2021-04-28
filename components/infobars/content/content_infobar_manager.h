// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_CONTENT_CONTENT_INFOBAR_MANAGER_H_
#define COMPONENTS_INFOBARS_CONTENT_CONTENT_INFOBAR_MANAGER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
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
// This class is not itself a WebContentsUserData in order to support such
// subclassing; it is expected that embedders will either have an instance of
// this class as a member of their "Tab" objects or create a custom subclass
// that is a WCUD.
class ContentInfoBarManager : public InfoBarManager,
                              public content::WebContentsObserver {
 public:
  explicit ContentInfoBarManager(content::WebContents* web_contents);
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
  // InfoBarManager:
  int GetActiveEntryID() override;

  // content::WebContentsObserver:
  void RenderProcessGone(base::TerminationStatus status) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;
  void WebContentsDestroyed() override;

  // See description in set_ignore_next_reload().
  bool ignore_next_reload_;

  DISALLOW_COPY_AND_ASSIGN(ContentInfoBarManager);
};

}  // namespace infobars

#endif  // COMPONENTS_INFOBARS_CONTENT_CONTENT_INFOBAR_MANAGER_H_
