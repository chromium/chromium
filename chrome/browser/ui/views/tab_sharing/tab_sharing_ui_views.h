// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_UI_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_UI_VIEWS_H_

#include <map>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/media/webrtc/same_origin_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_infobar_delegate.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_ui.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/tab_sharing/tab_capture_contents_border_helper.h"
#include "components/infobars/core/infobar_manager.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/models/image_model.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager_observer.h"
#endif

namespace content {
class WebContents;
}
namespace infobars {
class InfoBar;
}

class Profile;

class TabSharingUIViews : public TabSharingUI,
                          public BrowserListObserver,
                          public TabStripModelObserver,
                          public infobars::InfoBarManager::Observer,
#if BUILDFLAG(IS_CHROMEOS)
                          public policy::DlpContentManagerObserver,
#endif
                          public content::WebContentsObserver {
 public:
  TabSharingUIViews(content::GlobalRenderFrameHostId capturer,
                    const content::DesktopMediaID& media_id,
                    const std::u16string& capturer_name,
                    bool favicons_used_for_switch_to_tab_button,
                    bool app_preferred_current_tab,
                    TabSharingInfoBarDelegate::TabShareType capture_type,
                    bool captured_surface_control_active);
  ~TabSharingUIViews() override;

  // MediaStreamUI:
  // Called when tab sharing has started. Creates infobars on all tabs.
  gfx::NativeViewId OnStarted(
      base::OnceClosure stop_callback,
      content::MediaStreamUI::SourceCallback source_callback,
      const std::vector<content::DesktopMediaID>& media_ids) override;
  void OnRegionCaptureRectChanged(
      const std::optional<gfx::Rect>& region_capture_rect) override;

  // TabSharingUI:
  // Runs |source_callback_| to start sharing the tab containing |infobar|.
  // Removes infobars on all tabs; OnStarted() will recreate the infobars with
  // updated title and buttons.
  void StartSharing(infobars::InfoBar* infobar) override;

  // Runs |stop_callback_| to stop sharing |shared_tab_|. Removes infobars on
  // all tabs.
  void StopSharing() override;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;

  // InfoBarManager::Observer:
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;

  // WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void WebContentsDestroyed() override;
  // DidUpdateFaviconURL() is not overridden. We wait until
  // FaviconPeriodicUpdate() before updating the favicon. A captured tab can
  // toggle its favicon back and forth at an arbitrary rate, but we implicitly
  // rate-limit our response.

 protected:
#if BUILDFLAG(IS_CHROMEOS)
  // DlpContentManagerObserver:
  void OnConfidentialityChanged(
      policy::DlpRulesManager::Level old_restriction_level,
      policy::DlpRulesManager::Level new_restriction_level,
      content::WebContents* web_contents) override;
#endif

 private:
  using InfoBars = std::map<content::WebContents*,
                            raw_ptr<infobars::InfoBar, CtnExperimental>>;
  friend class TabSharingUIViewsBrowserTest;

  // Used to identify |TabSharingUIViews| instances to
  // |TabCaptureContentsBorderHelper|, without passing pointers,
  // which is less robust lifetime-wise.
  using CaptureSessionId = TabCaptureContentsBorderHelper::CaptureSessionId;

  // Observes the first invocation of a Captured Surface Control API by the
  // capturing tab and executes a once-callback.
  class CapturedSurfaceControlObserver : public content::WebContentsObserver {
   public:
    CapturedSurfaceControlObserver(content::WebContents* web_contents,
                                   base::OnceClosure callback);
    ~CapturedSurfaceControlObserver() override;

    // content::WebContentsObserver:
    void OnCapturedSurfaceControl() override;

   private:
    base::OnceClosure callback_;
  };

  enum class TabCaptureUpdate {
    kCaptureAdded,
    kCaptureRemoved,
    kCapturedVisibilityUpdated
  };

#if BUILDFLAG(IS_CHROMEOS)
  // Allows to test the DLP functionality of TabSharingUIViews even if the user
  // is not managed and without the need to initialize DlpRulesManager in tests.
  static void ApplyDlpForAllUsersForTesting();
#endif

  void CreateInfobarsForAllTabs();
  void CreateInfobarForWebContents(content::WebContents* contents);
  void RemoveInfobarsForAllTabs();

  void CreateTabCaptureIndicator();

  // Periodically checks for changes that would require the infobar to be
  // recreated, such as a favicon change.
  // Consult |share_session_seq_num_| for |share_session_seq_num|'s meaning.
  void FaviconPeriodicUpdate(size_t share_session_seq_num);

  void RefreshFavicons();

  void MaybeUpdateFavicon(content::WebContents* focus_target,
                          std::optional<uint32_t>* current_hash,
                          content::WebContents* infobar_owner);

  ui::ImageModel TabFavicon(content::WebContents* web_contents) const;
  ui::ImageModel TabFavicon(content::GlobalRenderFrameHostId rfh_id) const;

  void SetTabFaviconForTesting(content::WebContents* web_contents,
                               const ui::ImageModel& favicon);

  void StopCaptureDueToPolicy(content::WebContents* contents);

  void UpdateTabCaptureData(content::WebContents* contents,
                            TabCaptureUpdate update);

  // Whether the share-this-tab-instead button may be shown for |web_contents|.
  bool IsShareInsteadButtonPossible(content::WebContents* web_contents) const;

  // Tabs eligible for capture include:
  // * Tabs from the same profile.
  // * Tabs from an incognito profile may capture the original profile's tabs,
  //   and vice versa.
  // * Guest tabs may only capture other guest tabs. (Note that a guest tab's
  //   "original" session might be an arbitrary non-guest session.)
  bool IsCapturableByCapturer(const Profile* profile) const;

  // Invoked when the app in the capturing tab, which is observed by
  // `csc_observer_`, invokes a Captured Surface Control API for the first time
  // within the lifetime of `this` object.
  //
  // Note that `OnCapturedSurfaceControl()` is *NOT* overridden by
  // `TabSharingUIViews`, as `this` observes the captured tab,
  // whereas `csc_observer_` observes the capturing tab.
  void OnCapturedSurfaceControlByCapturer();

  // As for the purpose of this identification:
  // Assume a tab is captured twice, and both sessions use Region Capture.
  // The blue border falls back on its viewport-encompassing form. But when
  // one of these captures terminates, the blue border should track the
  // remaining session's crop-target.
  static CaptureSessionId next_capture_session_id_;
  const CaptureSessionId capture_session_id_;

  // The capturer's profile.
  const raw_ptr<Profile, DanglingUntriaged> profile_;

  InfoBars infobars_;
  std::map<content::WebContents*, std::unique_ptr<SameOriginObserver>>
      same_origin_observers_;
  const content::GlobalRenderFrameHostId capturer_;
  const url::Origin capturer_origin_;
  const bool can_focus_capturer_;
  const bool capturer_restricted_to_same_origin_ = false;
  content::DesktopMediaID shared_tab_media_id_;

  // Represents the web app name or the sink name receiving the captured stream.
  const std::u16string capturer_name_;

  raw_ptr<content::WebContents, DanglingUntriaged> shared_tab_;
  std::unique_ptr<SameOriginObserver> shared_tab_origin_observer_;
  std::u16string shared_tab_name_;
  std::unique_ptr<content::MediaStreamUI> tab_capture_indicator_ui_;

  // FaviconPeriodicUpdate() runs on a delayed task which re-posts itself.
  // The first task is associated with |share_session_seq_num_|, then all
  // repetitions of the task are associated with that value.
  // When |share_session_seq_num_| is incremented, all previously scheduled
  // tasks are invalidated, thereby ensuring that no more than one "live"
  // FaviconPeriodicUpdate() task can exist at any given moment.
  size_t share_session_seq_num_ = 0;

  content::MediaStreamUI::SourceCallback source_callback_;
  base::OnceClosure stop_callback_;

  // TODO(crbug.com/40188004): Re-enable favicons by default or drop the code.
  const bool favicons_used_for_switch_to_tab_button_;

  const bool app_preferred_current_tab_;

  // Indicates whether this instance is used for casting or capturing.
  const TabSharingInfoBarDelegate::TabShareType capture_type_;

  bool captured_surface_control_active_ = false;
  std::unique_ptr<CapturedSurfaceControlObserver> csc_observer_;

  std::optional<uint32_t> capturer_favicon_hash_;
  std::optional<uint32_t> captured_favicon_hash_;

  std::map<content::WebContents*, ui::ImageModel>
      favicon_overrides_for_testing_;

  base::WeakPtrFactory<TabSharingUIViews> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TAB_SHARING_TAB_SHARING_UI_VIEWS_H_
