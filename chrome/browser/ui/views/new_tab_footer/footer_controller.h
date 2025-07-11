// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NEW_TAB_FOOTER_FOOTER_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_NEW_TAB_FOOTER_FOOTER_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_web_view.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents_observer.h"

class BrowserWindowInterface;

namespace new_tab_footer {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum FooterNoticeItem {
  kExtensionAttribution = 0,
  kManagementNotice = 1,
  kMaxValue = kManagementNotice,
};

// Class used to manage the state of the new tab footer.
class NewTabFooterController : public content::WebContentsObserver {
 public:
  explicit NewTabFooterController(BrowserWindowInterface* browser,
                                  NewTabFooterWebView* footer);
  NewTabFooterController(const NewTabFooterController&) = delete;
  NewTabFooterController& operator=(const NewTabFooterController&) = delete;
  ~NewTabFooterController() override;

  void TearDown();

  void SkipErrorPageCheckForTesting(bool should_skip_check) {
    skip_error_page_check_for_testing_ = should_skip_check;
  }

 private:
  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void UpdateFooterVisibility(bool log_on_load_metric);
  // Callback for active tab changes from BrowserWindowInterface.
  void OnActiveTabChanged(BrowserWindowInterface* browser);
  bool ShouldSkipForErrorPage() const;
  bool ShouldShowExtensionFooter(const GURL& url);
  bool ShouldShowManagedFooter(const GURL& url);

  bool skip_error_page_check_for_testing_ = false;
  raw_ptr<BrowserWindowInterface> browser_;
  raw_ptr<new_tab_footer::NewTabFooterWebView> footer_;
  base::CallbackListSubscription tab_activation_subscription_;
  PrefChangeRegistrar pref_change_registrar_;
  PrefChangeRegistrar local_state_pref_change_registrar_;
  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<NewTabFooterController> weak_factory_{this};
};

}  // namespace new_tab_footer

#endif  // CHROME_BROWSER_UI_VIEWS_NEW_TAB_FOOTER_FOOTER_CONTROLLER_H_
