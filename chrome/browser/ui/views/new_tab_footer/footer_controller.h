// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NEW_TAB_FOOTER_FOOTER_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_NEW_TAB_FOOTER_FOOTER_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents_observer.h"

class ContentsContainerView;

namespace views {
class WebView;
}  // namespace views

namespace new_tab_footer {

class NewTabFooterControllerObserver;
class NewTabFooterWebView;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum FooterNoticeItem {
  kExtensionAttribution = 0,
  kManagementNotice = 1,
  kMaxValue = kManagementNotice,
};

// Class used to manage the state of new tab footers.
class NewTabFooterController {
 public:
  explicit NewTabFooterController(
      Profile* profile,
      std::vector<ContentsContainerView*> contents_container_views);
  NewTabFooterController(const NewTabFooterController&) = delete;
  NewTabFooterController& operator=(const NewTabFooterController&) = delete;
  ~NewTabFooterController();

  void TearDown();

  bool GetFooterVisible(content::WebContents* contents) const;

  void AddObserver(NewTabFooterControllerObserver* observer);
  void RemoveObserver(NewTabFooterControllerObserver* observer);

  // Controls a single NewTabFooterWebView. Updates the footer visibility when
  // the associated ContentsWebView changes web contents, or if its web contents
  // navigates.
  class ContentsViewFooterCotroller : public content::WebContentsObserver {
   public:
    ContentsViewFooterCotroller(NewTabFooterController* owner,
                                ContentsContainerView* contents_container_view);
    ContentsViewFooterCotroller(const ContentsViewFooterCotroller&) = delete;
    ContentsViewFooterCotroller& operator=(const ContentsViewFooterCotroller&) =
        delete;

    void OnWebContentsAttached(views::WebView* web_view);
    void OnWebContentsDetached(views::WebView* web_view);

    // content::WebContentsObserver:
    void DidFinishNavigation(
        content::NavigationHandle* navigation_handle) override;

    void UpdateFooterVisibility(bool log_on_load_metric);
    bool GetFooterVisible();
    bool ShouldSkipForErrorPage() const;
    bool ShouldShowExtensionFooter(const GURL& url);
    bool ShouldShowManagedFooter(const GURL& url);

   private:
    raw_ptr<NewTabFooterController> owner_;
    raw_ptr<NewTabFooterWebView> footer_;

    base::CallbackListSubscription web_contents_attached_subscription_;
    base::CallbackListSubscription web_contents_detached_subscription_;
  };

  void SkipErrorPageCheckForTesting(bool should_skip_check) {
    skip_error_page_check_for_testing_ = should_skip_check;
  }

 private:
  void UpdateFooterVisibilities(bool log_on_load_metric);

  bool skip_error_page_check_for_testing_ = false;
  std::vector<std::unique_ptr<ContentsViewFooterCotroller>> footer_controllers_;
  PrefChangeRegistrar pref_change_registrar_;
  PrefChangeRegistrar local_state_pref_change_registrar_;
  raw_ptr<Profile> profile_;

  base::ObserverList<NewTabFooterControllerObserver> observers_;

  base::WeakPtrFactory<NewTabFooterController> weak_factory_{this};
};

}  // namespace new_tab_footer

#endif  // CHROME_BROWSER_UI_VIEWS_NEW_TAB_FOOTER_FOOTER_CONTROLLER_H_
