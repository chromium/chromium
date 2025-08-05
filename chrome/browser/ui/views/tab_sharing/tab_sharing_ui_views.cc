// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_sharing/tab_sharing_ui_views.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/same_origin_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_infobar_delegate.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_ui.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/tab_sharing/tab_capture_contents_border_helper.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/constants.h"
#include "media/capture/capture_switches.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/policy/dlp/dlp_content_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/views/widget/native_widget_aura.h"
#endif

namespace {

using TabRole = ::TabSharingInfoBarDelegate::TabRole;
using content::GlobalRenderFrameHostId;
using content::RenderFrameHost;
using content::WebContents;

// Omit http:// and https:// url-schemes for the shared tab in the
// TabSharingInfoBar.
// This flag only has an effect if:
// - the TabCaptureInfobarLinks feature is enabled.
BASE_FEATURE(kTabSharingBarOmitHttpAndHttps,
             "TabSharingBarOmitHttpAndHttps",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Omit cryptographic url-schemes for the shared tab in the TabSharingInfoBar.
// This flag only has an effect if:
// - the TabCaptureInfobarLinks feature is enabled, and
// - the TabSharingBarOmitHttpAndHttps feature is disabled.
BASE_FEATURE(kTabSharingBarOmitCryptographic,
             "TabSharingBarOmitCryptographic",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_CHROMEOS)
bool g_apply_dlp_for_all_users_for_testing_ = false;
#endif

url_formatter::SchemeDisplay GetSharedTabSchemeDisplay() {
  if (!base::FeatureList::IsEnabled(features::kTabCaptureInfobarLinks)) {
    return url_formatter::SchemeDisplay::SHOW;
  }

  if (base::FeatureList::IsEnabled(kTabSharingBarOmitHttpAndHttps)) {
    return url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS;
  }

  if (base::FeatureList::IsEnabled(kTabSharingBarOmitCryptographic)) {
    return url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC;
  }

  return url_formatter::SchemeDisplay::SHOW;
}

std::u16string GetSharedTabName(
    WebContents* tab,
    const url_formatter::SchemeDisplay scheme_display) {
  const std::u16string formatted_origin =
      url_formatter::FormatOriginForSecurityDisplay(
          tab->GetPrimaryMainFrame()->GetLastCommittedOrigin(), scheme_display);
  return formatted_origin.empty() ? tab->GetTitle() : formatted_origin;
}

GlobalRenderFrameHostId GetGlobalId(WebContents* web_contents) {
  if (!web_contents) {
    return GlobalRenderFrameHostId();
  }
  auto* const main_frame = web_contents->GetPrimaryMainFrame();
  return main_frame ? main_frame->GetGlobalId() : GlobalRenderFrameHostId();
}

WebContents* WebContentsFromId(GlobalRenderFrameHostId rfh_id) {
  // Note that both FromID() and FromRenderFrameHost() are robust to null
  // input.
  return content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(rfh_id));
}

url::Origin GetOriginFromId(GlobalRenderFrameHostId rfh_id) {
  auto* rfh = content::RenderFrameHost::FromID(rfh_id);
  if (!rfh) {
    return {};
  }

  return rfh->GetLastCommittedOrigin();
}

bool CapturerRestrictedToSameOrigin(GlobalRenderFrameHostId capturer_id) {
  WebContents* capturer = WebContentsFromId(capturer_id);
  if (!capturer) {
    return false;
  }
  return capture_policy::GetAllowedCaptureLevel(
             GetOriginFromId(capturer_id).GetURL(), capturer) ==
         AllowedScreenCaptureLevel::kSameOrigin;
}

TabRole GetTabRole(bool is_capturing_tab, bool is_captured_tab) {
  if (is_capturing_tab && is_captured_tab) {
    return TabRole::kSelfCapturingTab;
  } else if (is_capturing_tab) {
    return TabRole::kCapturingTab;
  } else if (is_captured_tab) {
    return TabRole::kCapturedTab;
  } else {
    return TabRole::kOtherTab;
  }
}

}  // namespace

uint32_t TabSharingUIViews::next_capture_session_id_ = 0;

// static
std::unique_ptr<TabSharingUI> TabSharingUI::Create(
    GlobalRenderFrameHostId capturer,
    const content::DesktopMediaID& media_id,
    const std::u16string& capturer_name,
    bool app_preferred_current_tab,
    TabSharingInfoBarDelegate::TabShareType capture_type,
    bool captured_surface_control_active) {
  return std::make_unique<TabSharingUIViews>(
      capturer, media_id, capturer_name, app_preferred_current_tab,
      capture_type, captured_surface_control_active);
}

TabSharingUIViews::TabSharingUIViews(
    GlobalRenderFrameHostId capturer,
    const content::DesktopMediaID& media_id,
    const std::u16string& capturer_name,
    bool app_preferred_current_tab,
    TabSharingInfoBarDelegate::TabShareType capture_type,
    bool captured_surface_control_active)
    : capture_session_id_(next_capture_session_id_++),
      profile_(ProfileManager::GetLastUsedProfileAllowedByPolicy()),
      capturer_(capturer),
      capturer_origin_(GetOriginFromId(capturer)),
      can_focus_capturer_(GetOriginFromId(capturer).scheme() !=
                          extensions::kExtensionScheme),
      capturer_restricted_to_same_origin_(
          CapturerRestrictedToSameOrigin(capturer)),
      shared_tab_media_id_(media_id),
      capturer_name_(std::move(capturer_name)),
      shared_tab_(WebContents::FromRenderFrameHost(RenderFrameHost::FromID(
          media_id.web_contents_id.render_process_id,
          media_id.web_contents_id.main_render_frame_id))),
      shared_tab_scheme_display_(GetSharedTabSchemeDisplay()),
      app_preferred_current_tab_(app_preferred_current_tab),
      capture_type_(capture_type),
      captured_surface_control_active_(captured_surface_control_active) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Observe(shared_tab_);
  shared_tab_name_ = GetSharedTabName(shared_tab_, shared_tab_scheme_display_);

  if (capturer_restricted_to_same_origin_) {
    // base::Unretained is safe here because we own the origin observer, so it
    // cannot outlive us.
    shared_tab_origin_observer_ = std::make_unique<SameOriginObserver>(
        shared_tab_, capturer_origin_,
        base::BindRepeating(&TabSharingUIViews::StopCaptureDueToPolicy,
                            base::Unretained(this)));
  }

  WebContents* const capturer_wc = WebContentsFromId(capturer_);
  if (capturer_wc) {
    // base::Unretained(this) is safe because `this` owns `csc_observer_` and
    // will outlive it.
    csc_observer_ = std::make_unique<CapturedSurfaceControlObserver>(
        capturer_wc,
        base::BindOnce(&TabSharingUIViews::OnCapturedSurfaceControlByCapturer,
                       base::Unretained(this)));
  }
}

TabSharingUIViews::~TabSharingUIViews() {
  // Unconditionally call StopSharing(), to ensure all clean-up has been
  // performed if tasks race (e.g., OnStarted() is called after
  // OnInfoBarRemoved()). See: https://crbug.com/1155426
  StopSharing();
}

gfx::NativeViewId TabSharingUIViews::OnStarted(
    base::OnceClosure stop_callback,
    content::MediaStreamUI::SourceCallback source_callback,
    const std::vector<content::DesktopMediaID>& media_ids) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  source_callback_ = std::move(source_callback);
  stop_callback_ = std::move(stop_callback);
  CreateInfobarsForAllTabs();
  UpdateTabCaptureData(shared_tab_, TabCaptureUpdate::kCaptureAdded);
  CreateTabCaptureIndicator();
  return 0;
}

void TabSharingUIViews::OnRegionCaptureRectChanged(
    const std::optional<gfx::Rect>& region_capture_rect) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!shared_tab_) {
    return;
  }

  auto* const helper =
      TabCaptureContentsBorderHelper::FromWebContents(shared_tab_);
  if (!helper) {
    return;
  }

  helper->OnRegionCaptureRectChanged(capture_session_id_, region_capture_rect);
}

void TabSharingUIViews::StartSharing(infobars::InfoBar* infobar) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (source_callback_.is_null()) {
    return;
  }

  WebContents* shared_tab =
      infobars::ContentInfoBarManager::WebContentsFromInfoBar(infobar);
  DCHECK(shared_tab);
  DCHECK_EQ(infobars_[shared_tab], infobar);

  RenderFrameHost* main_frame = shared_tab->GetPrimaryMainFrame();
  DCHECK(main_frame);
  source_callback_.Run(
      content::DesktopMediaID(content::DesktopMediaID::TYPE_WEB_CONTENTS,
                              content::DesktopMediaID::kNullId,
                              content::WebContentsMediaCaptureId(
                                  main_frame->GetProcess()->GetDeprecatedID(),
                                  main_frame->GetRoutingID())),
      captured_surface_control_active_);
}

void TabSharingUIViews::StopSharing() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!stop_callback_.is_null()) {
    std::move(stop_callback_).Run();
  }
#if BUILDFLAG(IS_CHROMEOS)
  policy::DlpContentManager::Get()->RemoveObserver(
      this, policy::DlpContentRestriction::kScreenShare);
#endif
  RemoveInfobarsForAllTabs();
  UpdateTabCaptureData(shared_tab_, TabCaptureUpdate::kCaptureRemoved);
  tab_capture_indicator_ui_.reset();
  shared_tab_ = nullptr;
}

void TabSharingUIViews::OnBrowserAdded(Browser* browser) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(browser);

  if (IsCapturableByCapturer(browser->profile())) {
    browser->tab_strip_model()->AddObserver(this);
  }
}

void TabSharingUIViews::OnBrowserRemoved(Browser* browser) {
  BrowserList* browser_list = BrowserList::GetInstance();
  if (browser_list->empty()) {
    browser_list->RemoveObserver(this);
  }
  browser->tab_strip_model()->RemoveObserver(this);
}

void TabSharingUIViews::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& contents : change.GetInsert()->contents) {
      if (infobars_.find(contents.contents) == infobars_.end()) {
        CreateInfobarForWebContents(contents.contents);
      }
    }
  }

  if (change.type() == TabStripModelChange::kRemoved) {
    for (const auto& contents : change.GetRemove()->contents) {
      same_origin_observers_.erase(contents.contents);
    }
  }

  if (selection.active_tab_changed()) {
    UpdateTabCaptureData(selection.old_contents,
                         TabCaptureUpdate::kCapturedVisibilityUpdated);
    UpdateTabCaptureData(selection.new_contents,
                         TabCaptureUpdate::kCapturedVisibilityUpdated);
  }
}

void TabSharingUIViews::TabChangedAt(WebContents* contents,
                                     int index,
                                     TabChangeType change_type) {
  // Sad tab cannot be shared so don't create an infobar for it.
  auto* sad_tab_helper = SadTabHelper::FromWebContents(contents);
  if (sad_tab_helper && sad_tab_helper->sad_tab()) {
    return;
  }

  if (infobars_.find(contents) == infobars_.end()) {
    CreateInfobarForWebContents(contents);
  }
}

void TabSharingUIViews::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                         bool animate) {
  auto infobars_entry =
      std::ranges::find(infobars_, infobar, &InfoBars::value_type::second);
  if (infobars_entry == infobars_.end()) {
    return;
  }

  infobar->owner()->RemoveObserver(this);
  infobars_.erase(infobars_entry);
  if (infobars::ContentInfoBarManager::WebContentsFromInfoBar(infobar) ==
      shared_tab_) {
    StopSharing();
  }
}

void TabSharingUIViews::PrimaryPageChanged(content::Page& page) {
  if (!shared_tab_) {
    return;
  }
  shared_tab_name_ = GetSharedTabName(shared_tab_, shared_tab_scheme_display_);
  for (const auto& infobars_entry : infobars_) {
    // Recreate infobars to reflect the new shared tab's hostname.
    if (infobars_entry.first != shared_tab_) {
      CreateInfobarForWebContents(infobars_entry.first);
    }
  }
}

void TabSharingUIViews::WebContentsDestroyed() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(crbug.com/40207587): Prevent StopSharing() from interacting with
  // |shared_tab_| while it is being destroyed.
  StopSharing();
}

#if BUILDFLAG(IS_CHROMEOS)
void TabSharingUIViews::OnConfidentialityChanged(
    policy::DlpRulesManager::Level old_restriction_level,
    policy::DlpRulesManager::Level new_restriction_level,
    content::WebContents* web_contents) {
  DCHECK(old_restriction_level != new_restriction_level);
  if (old_restriction_level == policy::DlpRulesManager::Level::kBlock ||
      new_restriction_level == policy::DlpRulesManager::Level::kBlock) {
    // We only call this function if it was previously blocked or should be
    // blocked now.
    CreateInfobarForWebContents(web_contents);
  }
}

// static
void TabSharingUIViews::ApplyDlpForAllUsersForTesting() {
  g_apply_dlp_for_all_users_for_testing_ = true;
}
#endif

void TabSharingUIViews::CreateInfobarsForAllTabs() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  BrowserList* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    CHECK(browser);

    if (!IsCapturableByCapturer(browser->profile())) {
      continue;
    }

    OnBrowserAdded(browser);

    TabStripModel* tab_strip_model = browser->tab_strip_model();
    for (int i = 0; i < tab_strip_model->count(); i++) {
      CreateInfobarForWebContents(tab_strip_model->GetWebContentsAt(i));
    }
  }
  browser_list->AddObserver(this);
#if BUILDFLAG(IS_CHROMEOS)
  // Observe only for managed users.
  if (g_apply_dlp_for_all_users_for_testing_ ||
      policy::DlpRulesManagerFactory::GetForPrimaryProfile()) {
    policy::DlpContentManager::Get()->AddObserver(
        this, policy::DlpContentRestriction::kScreenShare);
  }
#endif
}

void TabSharingUIViews::CreateInfobarForWebContents(WebContents* contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(contents);

  // Don't show the info bar in a Picture in Picture window, since it doesn't
  // typically fit anyway.
  Browser* browser = chrome::FindBrowserWithTab(contents);
  if (browser && browser->is_type_picture_in_picture()) {
    return;
  }

  infobars::InfoBar* old_infobar = nullptr;
  auto infobars_entry = infobars_.find(contents);
  // Stop observing the previous infobar instance if it already exists.
  if (infobars_entry != infobars_.end()) {
    old_infobar = infobars_entry->second;
    old_infobar->owner()->RemoveObserver(this);
  }
  auto* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(contents);
  infobar_manager->AddObserver(this);

  const bool is_capturing_tab = (GetGlobalId(contents) == capturer_);
  const bool is_captured_tab = (contents == shared_tab_);
  const bool is_share_instead_button_possible =
      IsShareInsteadButtonPossible(contents);

  // If sharing this tab instead of the currently captured tab is possible, it
  // may still be blocked by enterprise policy. If the enterprise policy is
  // active, create an observer that will inform us when its compliance state
  // changes.
  if (capturer_restricted_to_same_origin_ && is_share_instead_button_possible &&
      !base::Contains(same_origin_observers_, contents)) {
    // We explicitly remove all infobars and clear all policy observers before
    // destruction, so base::Unretained is safe here.
    same_origin_observers_[contents] = std::make_unique<SameOriginObserver>(
        contents, capturer_origin_,
        base::BindRepeating(&TabSharingUIViews::CreateInfobarForWebContents,
                            base::Unretained(this)));
  }

  content::GlobalRenderFrameHostId focus_target;
  if (can_focus_capturer_) {
    // Self-capture -> no switch-to button.
    // Capturer -> switch-to-captured.
    // Captured -> switch-to-capturer.
    // Otherwise -> no switch-to button.
    if (is_capturing_tab && !is_captured_tab) {
      focus_target = GetGlobalId(shared_tab_);
    } else if (!is_capturing_tab && is_captured_tab) {
      focus_target = capturer_;
    }
  }

  // Determine if we are currently allowed to share this tab by policy.
  bool is_sharing_allowed_by_policy =
      !capturer_restricted_to_same_origin_ ||
      capturer_origin_.IsSameOriginWith(
          contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());

#if BUILDFLAG(IS_CHROMEOS)
  // Check if dlp policies allow sharing.
  // This check is skipped if sharing is already forbidden.
  if (is_sharing_allowed_by_policy) {
    const bool dlp_enabled =
        g_apply_dlp_for_all_users_for_testing_ ||
        policy::DlpRulesManagerFactory::GetForPrimaryProfile();
    if (dlp_enabled &&
        policy::DlpContentManager::Get()->IsScreenShareBlocked(contents)) {
      is_sharing_allowed_by_policy = false;
    }
  }
#endif

  TabSharingInfoBarDelegate::ButtonState share_this_tab_instead_button_state =
      !is_share_instead_button_possible
          ? TabSharingInfoBarDelegate::ButtonState::NOT_SHOWN
      : is_sharing_allowed_by_policy
          ? TabSharingInfoBarDelegate::ButtonState::ENABLED
          : TabSharingInfoBarDelegate::ButtonState::DISABLED;

  infobars_[contents] = TabSharingInfoBarDelegate::Create(
      infobar_manager, old_infobar, GetGlobalId(shared_tab_), capturer_,
      shared_tab_name_, capturer_name_, contents,
      GetTabRole(is_capturing_tab, is_captured_tab),
      share_this_tab_instead_button_state, focus_target,
      captured_surface_control_active_, this, capture_type_);
}

void TabSharingUIViews::RemoveInfobarsForAllTabs() {
  BrowserList::GetInstance()->RemoveObserver(this);
  TabStripModelObserver::StopObservingAll(this);

  for (const auto& infobars_entry : infobars_) {
    infobars_entry.second->owner()->RemoveObserver(this);
    infobars_entry.second->RemoveSelf();
  }

  infobars_.clear();
  same_origin_observers_.clear();
}

void TabSharingUIViews::CreateTabCaptureIndicator() {
  const blink::MediaStreamDevice device(
      blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE,
      shared_tab_media_id_.ToString(), std::string());
  if (!shared_tab_) {
    return;
  }

  blink::mojom::StreamDevices devices;
  devices.video_device = device;
  tab_capture_indicator_ui_ = MediaCaptureDevicesDispatcher::GetInstance()
                                  ->GetMediaStreamCaptureIndicator()
                                  ->RegisterMediaStream(shared_tab_, devices);
  tab_capture_indicator_ui_->OnStarted(
      /*stop=*/base::DoNothing(), content::MediaStreamUI::SourceCallback(),
      /*label=*/std::string(), /*screen_capture_ids=*/{},
      content::MediaStreamUI::StateChangeCallback());
}

void TabSharingUIViews::StopCaptureDueToPolicy(content::WebContents* contents) {
  DCHECK(shared_tab_ == contents);
  StopSharing();
  // We use |contents| rather than |shared_tab_| here because |shared_tab_| is
  // cleared by the call to StopSharing().
  capture_policy::ShowCaptureTerminatedDialog(contents);
}

void TabSharingUIViews::UpdateTabCaptureData(WebContents* contents,
                                             TabCaptureUpdate update) {
  if (!contents) {
    return;
  }

  TabCaptureContentsBorderHelper::CreateForWebContents(contents);
  auto* const helper =
      TabCaptureContentsBorderHelper::FromWebContents(contents);

  switch (update) {
    case TabCaptureUpdate::kCaptureAdded:
      helper->OnCapturerAdded(capture_session_id_);
      break;
    case TabCaptureUpdate::kCaptureRemoved:
      helper->OnCapturerRemoved(capture_session_id_);
      break;
    case TabCaptureUpdate::kCapturedVisibilityUpdated:
      helper->VisibilityUpdated();
      break;
  }
}

bool TabSharingUIViews::IsShareInsteadButtonPossible(
    content::WebContents* web_contents) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (source_callback_.is_null()) {
    // No callback to support share-this-tab-instead.
    // This can happen, for instance, if the application specifies
    // {surfaceSwitching: "exclude"}.
    return false;
  }

  if (web_contents == shared_tab_) {
    return false;  // |web_contents| is already the shared tab.
  }

  if (GetGlobalId(web_contents) != capturer_) {
    return true;  // Any tab other than the capturing/captured tab is eligible.
  }

  // If the application specified {preferCurrentTab: true}, we detect that
  // the current tab is a reasonable choice. We therefore expose the button
  // that lets the user switch to sharing the current tab.
  //
  // Note that for many applications, choosing the current tab is undesirable.
  // For example, in the context of video-conferencing applications, it would
  // often produce a "hall of mirrors" effect.
  return app_preferred_current_tab_;
}

bool TabSharingUIViews::IsCapturableByCapturer(const Profile* profile) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(profile);

  // Guest profiles may have an arbitrary non-guest profile as their original,
  // so direct comparison would not work. Instead, we rely on the assumption
  // that there is at most one guest profile.
  const bool capturer_is_guest = profile_ && profile_->IsGuestSession();
  const bool new_is_guest = profile->IsGuestSession();
  if (capturer_is_guest || new_is_guest) {
    return capturer_is_guest && new_is_guest;
  }

  return profile->GetOriginalProfile() == profile_;
}

void TabSharingUIViews::OnCapturedSurfaceControlByCapturer() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!captured_surface_control_active_) {
    captured_surface_control_active_ = true;

    content::WebContents* const capturer_wc = WebContentsFromId(capturer_);
    if (capturer_wc) {
      // Recreate the infobar with the CSC indicator in place - triggered by
      // `captured_surface_control_active_` marking CSC as "active".
      CreateInfobarForWebContents(capturer_wc);
    } else {
      // The capturer died - `TabSharingUIViews` will be destroyed promptly, so
      // no need to do anything special.
    }
  }
}

TabSharingUIViews::CapturedSurfaceControlObserver::
    CapturedSurfaceControlObserver(content::WebContents* web_contents,
                                   base::OnceClosure callback)
    : callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(callback_);

  Observe(web_contents);
}

TabSharingUIViews::CapturedSurfaceControlObserver::
    ~CapturedSurfaceControlObserver() = default;

void TabSharingUIViews::CapturedSurfaceControlObserver::
    OnCapturedSurfaceControl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Observe(nullptr);

  if (callback_) {
    std::move(callback_).Run();
  }
}
