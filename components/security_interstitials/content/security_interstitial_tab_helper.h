// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SECURITY_INTERSTITIAL_TAB_HELPER_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SECURITY_INTERSTITIAL_TAB_HELPER_H_

#include <map>

#include "components/security_interstitials/core/common/mojom/interstitial_commands.mojom.h"
#include "components/security_interstitials/core/controller_client.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
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
  // Enum representing how users close the security interstitial. These values
  // are persisted to logs. Entries should not be renumbered and numeric values
  // should never be reused.
  enum class InterstitialCloseReason {
    // An interstitial is shown.
    INTERSTITIAL_SHOWN = 0,
    // User navigates away from the interstitial on the same tab by clicking
    // Back to Safety, visiting unsafe website or entering a new URL in the
    // omnibox.
    NAVIGATE_AWAY = 1,
    // User closes the tab that is displaying the interstitial.
    CLOSE_TAB = 2,

    kMaxValue = CLOSE_TAB
  };

  SecurityInterstitialTabHelper(const SecurityInterstitialTabHelper&) = delete;
  SecurityInterstitialTabHelper& operator=(
      const SecurityInterstitialTabHelper&) = delete;

  ~SecurityInterstitialTabHelper() override;

  // WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;
  void FrameDeleted(content::FrameTreeNodeId frame_tree_node_id) override;

  // Associates |blocking_page| with an SecurityInterstitialTabHelper for the
  // given |navigation_handle|, to manage the |blocking_page|'s lifetime. This
  // method has no effect if called with a |navigation_handle| indicating
  // pre-rendering navigation.
  static void AssociateBlockingPage(
      content::NavigationHandle* navigation_handle,
      std::unique_ptr<security_interstitials::SecurityInterstitialPage>
          blocking_page);

  // Binds a receiver to the instance associated with the RenderFrameHost.
  static void BindInterstitialCommands(
      mojo::PendingAssociatedReceiver<
          security_interstitials::mojom::InterstitialCommands> receiver,
      content::RenderFrameHost* rfh);

  // Determines whether a URL should be shown on the current navigation page.
  // This is only false in cases that the main frame is displaying an
  // interstitial that requires not showing the URL.
  bool ShouldDisplayURL() const;

  // Whether this tab helper is tracking a currently-displaying interstitial in
  // any frame.
  bool IsDisplayingInterstitial() const;

  // Whether this tab has a pending interstitial that is not yet committed, or
  // it is currently displaying an interstitial in any frame.
  bool HasPendingOrActiveInterstitial() const;

  // Whether an interstitial has been associated for |navigation_id|, but hasn't
  // committed yet. For checking if the interstitial has committed use
  // IsDisplayingInterstitial.
  bool IsInterstitialPendingForNavigation(int64_t navigation_id) const;

  // Whether an interstitial has been committed for |frame_tree_node_id|.
  bool IsInterstitialCommittedForFrame(
      content::FrameTreeNodeId frame_tree_node_id) const;

  // Returns the blocking document for the main frame if its interstitial has
  // been committed.
  security_interstitials::SecurityInterstitialPage*
  GetBlockingPageForCurrentlyCommittedNavigationForTesting();

 private:
  explicit SecurityInterstitialTabHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<SecurityInterstitialTabHelper>;

  // Retrieves the blocking page for the target frame for which a Mojo command
  // is being processed. Calling this method when a Mojo command is not on the
  // stack will result in a crash.
  SecurityInterstitialPage* GetBlockingPageForCurrentTargetFrame();

  SecurityInterstitialPage* GetBlockingPageForMainFrame() const;

  void SetBlockingPage(
      int64_t navigation_id,
      std::unique_ptr<security_interstitials::SecurityInterstitialPage>
          blocking_page);

  // Sends the command to the currently active target frame to process.
  // This method must only be used when processing a Mojo command from
  // security_interstitials::mojom::InterstitialCommands:: implementation
  // otherwise this will result in a crash.
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
  void OpenEnhancedProtectionSettings() override;

  // Keeps track of blocking documents for pending navigations that have
  // encountered certificate errors in this WebContents. This map is keyed by
  // navigation ID rather than FrameTreeNodeId because there may be multiple
  // pending navigations per frame at the same time. When a navigation commits,
  // the corresponding blocking document is moved out and stored in
  // |blocking_documents_for_committed_navigations_|.
  std::map<int64_t,
           std::unique_ptr<security_interstitials::SecurityInterstitialPage>>
      blocking_documents_for_pending_navigations_;

  // Keeps track of the blocking document for each frame that has a committed
  // navigation. The value is replaced (if the new committed navigation for that
  // frame has a blocking document).
  std::map<content::FrameTreeNodeId,
           std::unique_ptr<security_interstitials::SecurityInterstitialPage>>
      blocking_documents_for_committed_navigations_;

  content::RenderFrameHostReceiverSet<
      security_interstitials::mojom::InterstitialCommands>
      receivers_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SECURITY_INTERSTITIAL_TAB_HELPER_H_
