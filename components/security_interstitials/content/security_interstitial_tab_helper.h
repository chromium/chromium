// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SECURITY_INTERSTITIAL_TAB_HELPER_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SECURITY_INTERSTITIAL_TAB_HELPER_H_

#include <map>

#include "components/security_interstitials/core/common/mojom/interstitial_commands.mojom.h"
#include "components/security_interstitials/core/controller_client.h"
#include "content/public/browser/web_contents_binding_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace security_interstitials {
class SecurityInterstitialPage;

// Long-lived helper associated with a WebContents, for owning blocking pages.
class SecurityInterstitialTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SecurityInterstitialTabHelper>,
      public security_interstitials::mojom::InterstitialCommands {
 public:
  ~SecurityInterstitialTabHelper() override;

  // WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  // Associates |blocking_page| with an SecurityInterstitialTabHelper for the
  // given |web_contents| and |navigation_id|, to manage the |blocking_page|'s
  // lifetime.
  static void AssociateBlockingPage(
      content::WebContents* web_contents,
      int64_t navigation_id,
      std::unique_ptr<security_interstitials::SecurityInterstitialPage>
          blocking_page);

  // Determines whether a URL should be shown on the current navigation page.
  bool ShouldDisplayURL() const;

  // Whether this tab helper is tracking a currently-displaying interstitial.
  bool IsDisplayingInterstitial() const;

  security_interstitials::SecurityInterstitialPage*
  GetBlockingPageForCurrentlyCommittedNavigationForTesting();

 private:
  explicit SecurityInterstitialTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<SecurityInterstitialTabHelper>;

  void SetBlockingPage(
      int64_t navigation_id,
      std::unique_ptr<security_interstitials::SecurityInterstitialPage>
          blocking_page);

  void HandleCommand(security_interstitials::SecurityInterstitialCommand cmd);

  // security_interstitials::mojom::InterstitialCommands::
  void DontProceed() override;
  void Proceed() override;
  void ShowMoreSection() override;
  void OpenHelpCenter() override;
  void OpenDiagnostic() override;
  void Reload() override;
  void OpenDateSettings() override;
  void OpenLogin() override;
  void DoReport() override;
  void DontReport() override;
  void OpenReportingPrivacy() override;
  void OpenWhitepaper() override;
  void ReportPhishingError() override;

  // Keeps track of blocking pages for navigations that have encountered
  // certificate errors in this WebContents. When a navigation commits, the
  // corresponding blocking page is moved out and stored in
  // |blocking_page_for_currently_committed_navigation_|.
  std::map<int64_t,
           std::unique_ptr<security_interstitials::SecurityInterstitialPage>>
      blocking_pages_for_navigations_;
  // Keeps track of the blocking page for the current committed navigation, if
  // there is one. The value is replaced (if the new committed navigation has a
  // blocking page) or reset on every committed navigation.
  std::unique_ptr<security_interstitials::SecurityInterstitialPage>
      blocking_page_for_currently_committed_navigation_;

  content::WebContentsFrameBindingSet<
      security_interstitials::mojom::InterstitialCommands>
      binding_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(SecurityInterstitialTabHelper);
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SECURITY_INTERSTITIAL_TAB_HELPER_H_
