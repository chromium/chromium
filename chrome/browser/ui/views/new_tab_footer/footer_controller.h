// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_NEW_TAB_FOOTER_FOOTER_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_NEW_TAB_FOOTER_FOOTER_CONTROLLER_H_

#include "chrome/browser/ui/views/new_tab_footer/footer_web_view.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"

namespace new_tab_footer {

// Class used to manage the state of the new tab footer.
class NewTabFooterController : public content::WebContentsObserver {
 public:
  explicit NewTabFooterController(tabs::TabInterface* tab);
  NewTabFooterController(const NewTabFooterController&) = delete;
  NewTabFooterController& operator=(const NewTabFooterController&) = delete;
  ~NewTabFooterController() override;

 private:
  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void UpdateFooterVisibility();
  // Called when the associated tab enters the foreground.
  void TabForegrounded(tabs::TabInterface* tab);
  void ShowUI();
  void CloseUI();

  const raw_ptr<tabs::TabInterface> tab_;
  raw_ptr<new_tab_footer::NewTabFooterWebView> footer_web_view_;
  base::CallbackListSubscription tab_did_activate_callback_subscription_;
  PrefChangeRegistrar pref_change_registrar_;
  PrefChangeRegistrar local_state_pref_change_registrar_;
  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<NewTabFooterController> weak_factory_{this};
};

}  // namespace new_tab_footer

#endif  // CHROME_BROWSER_UI_VIEWS_NEW_TAB_FOOTER_FOOTER_CONTROLLER_H_
