// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_sharing/tab_sharing_ui_views.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/same_origin_observer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_infobar_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/border.h"

#if defined(OS_WIN)
#include "ui/views/widget/native_widget_aura.h"
#endif

namespace {

using content::GlobalRenderFrameHostId;
using content::RenderFrameHost;
using content::WebContents;

#if !BUILDFLAG(IS_CHROMEOS_ASH)
const int kContentsBorderThickness = 5;
const float kContentsBorderOpacity = 0.50;
const SkColor kContentsBorderColor = gfx::kGoogleBlue500;

void InitContentsBorderWidget(WebContents* contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  if (!browser)
    return;
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (browser_view->contents_border_widget())
    return;

  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  views::Widget* frame = browser_view->contents_web_view()->GetWidget();
  params.parent = frame->GetNativeView();
  params.context = frame->GetNativeWindow();
  // Make the widget non-top level.
  params.child = true;
  params.name = "TabSharingContentsBorder";
  params.remove_standard_frame = true;
  // Let events go through to underlying view.
  params.accept_events = false;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
#if defined(OS_WIN)
  params.native_widget = new views::NativeWidgetAura(widget);
#endif

  widget->Init(std::move(params));
  auto border_view = std::make_unique<views::View>();
  border_view->SetBorder(
      views::CreateSolidBorder(kContentsBorderThickness, kContentsBorderColor));
  widget->SetContentsView(std::move(border_view));
  widget->SetVisibilityChangedAnimationsEnabled(false);
  widget->SetOpacity(kContentsBorderOpacity);

  browser_view->set_contents_border_widget(widget);
}
#endif

void SetContentsBorderVisible(WebContents* contents, bool visible) {
  // TODO(https://crbug.com/1030925) fix contents border on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (!contents)
    return;
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  if (!browser)
    return;

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view->contents_border_widget()) {
    if (!visible)
      return;
    InitContentsBorderWidget(contents);
  }
  views::Widget* contents_border_widget =
      browser_view->contents_border_widget();
  if (visible)
    contents_border_widget->Show();
  else
    contents_border_widget->Hide();
#endif
}

std::u16string GetTabName(WebContents* tab) {
  const GURL& url = tab->GetLastCommittedURL();
  const std::u16string tab_name =
      network::IsUrlPotentiallyTrustworthy(url)
          ? base::UTF8ToUTF16(net::GetHostAndOptionalPort(url))
          : url_formatter::FormatUrlForSecurityDisplay(url.GetOrigin());
  return tab_name.empty() ? tab->GetTitle() : tab_name;
}

GlobalRenderFrameHostId GetGlobalId(WebContents* web_contents) {
  auto* const main_frame = web_contents->GetMainFrame();
  return main_frame ? main_frame->GetGlobalId() : GlobalRenderFrameHostId();
}

uint32_t GetHash(const ui::ImageModel& image) {
  if (image.IsEmpty()) {
    return 0;
  }

  const SkBitmap* const bitmap = image.GetImage().ToSkBitmap();
  if (!bitmap) {
    return 0;
  }

  return base::FastHash(base::make_span(
      static_cast<uint8_t*>(bitmap->getPixels()), bitmap->computeByteSize()));
}

WebContents* WebContentsFromId(GlobalRenderFrameHostId rfh_id) {
  // Note that both FromID() and FromRenderFrameHost() are robust to null
  // input.
  return content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(rfh_id));
}

GURL GetOriginFromId(GlobalRenderFrameHostId rfh_id) {
  WebContents* capturer = WebContentsFromId(rfh_id);
  if (!capturer)
    return {};

  return capturer->GetLastCommittedURL().GetOrigin();
}

bool CanFocusCapturer(GlobalRenderFrameHostId capturer_id) {
  WebContents* const capturer = WebContentsFromId(capturer_id);
  if (!capturer) {
    return false;
  }

  return !capturer->GetLastCommittedURL().SchemeIs(
      extensions::kExtensionScheme);
}

bool CapturerRestrictedToSameOrigin(GlobalRenderFrameHostId capturer_id) {
  WebContents* capturer = WebContentsFromId(capturer_id);
  if (!capturer)
    return false;
  return capture_policy::GetAllowedCaptureLevel(
             capturer->GetLastCommittedURL().GetOrigin(), capturer) ==
         AllowedScreenCaptureLevel::kSameOrigin;
}

}  // namespace

// static
std::unique_ptr<TabSharingUI> TabSharingUI::Create(
    GlobalRenderFrameHostId capturer,
    const content::DesktopMediaID& media_id,
    std::u16string app_name,
    bool favicons_used_for_switch_to_tab_button) {
  return base::WrapUnique(new TabSharingUIViews(
      capturer, media_id, app_name, favicons_used_for_switch_to_tab_button));
}

TabSharingUIViews::TabSharingUIViews(
    GlobalRenderFrameHostId capturer,
    const content::DesktopMediaID& media_id,
    std::u16string app_name,
    bool favicons_used_for_switch_to_tab_button)
    : capturer_(capturer),
      capturer_origin_(GetOriginFromId(capturer)),
      can_focus_capturer_(CanFocusCapturer(capturer)),
      capturer_restricted_to_same_origin_(
          CapturerRestrictedToSameOrigin(capturer)),
      shared_tab_media_id_(media_id),
      app_name_(std::move(app_name)),
      favicons_used_for_switch_to_tab_button_(
          favicons_used_for_switch_to_tab_button) {
  shared_tab_ = WebContents::FromRenderFrameHost(
      RenderFrameHost::FromID(media_id.web_contents_id.render_process_id,
                              media_id.web_contents_id.main_render_frame_id));
  Observe(shared_tab_);
  shared_tab_name_ = GetTabName(shared_tab_);
  profile_ = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  // TODO(https://crbug.com/1030925) fix contents border on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  InitContentsBorderWidget(shared_tab_);
#endif

  if (capturer_restricted_to_same_origin_) {
    // base::Unretained is safe here because we own the origin observer, so it
    // cannot outlive us.
    shared_tab_origin_observer_ = std::make_unique<SameOriginObserver>(
        shared_tab_, capturer_origin_,
        base::BindRepeating(&TabSharingUIViews::StopCaptureDueToPolicy,
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
  SetContentsBorderVisible(shared_tab_, true);
  CreateTabCaptureIndicator();
  if (favicons_used_for_switch_to_tab_button_) {
    FaviconPeriodicUpdate(++share_session_seq_num_);
  }
  return 0;
}

void TabSharingUIViews::StartSharing(infobars::InfoBar* infobar) {
  if (source_callback_.is_null())
    return;

  SetContentsBorderVisible(shared_tab_, false);

  WebContents* shared_tab =
      infobars::ContentInfoBarManager::WebContentsFromInfoBar(infobar);
  DCHECK(shared_tab);
  DCHECK_EQ(infobars_[shared_tab], infobar);
  shared_tab_ = shared_tab;
  shared_tab_name_ = GetTabName(shared_tab_);

  RenderFrameHost* main_frame = shared_tab->GetMainFrame();
  DCHECK(main_frame);
  RemoveInfobarsForAllTabs();
  source_callback_.Run(content::DesktopMediaID(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(main_frame->GetProcess()->GetID(),
                                         main_frame->GetRoutingID())));
}

void TabSharingUIViews::StopSharing() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!stop_callback_.is_null())
    std::move(stop_callback_).Run();
  RemoveInfobarsForAllTabs();
  SetContentsBorderVisible(shared_tab_, false);
  tab_capture_indicator_ui_.reset();
  shared_tab_ = nullptr;
  ++share_session_seq_num_;  // Invalidates previously scheduled tasks.
}

void TabSharingUIViews::OnBrowserAdded(Browser* browser) {
  if (browser->profile()->GetOriginalProfile() == profile_)
    browser->tab_strip_model()->AddObserver(this);
}

void TabSharingUIViews::OnBrowserRemoved(Browser* browser) {
  BrowserList* browser_list = BrowserList::GetInstance();
  if (browser_list->empty())
    browser_list->RemoveObserver(this);
  browser->tab_strip_model()->RemoveObserver(this);
}

void TabSharingUIViews::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& contents : change.GetInsert()->contents) {
      if (infobars_.find(contents.contents) == infobars_.end())
        CreateInfobarForWebContents(contents.contents);
    }
  }

  if (selection.active_tab_changed()) {
    if (selection.old_contents)
      SetContentsBorderVisible(selection.old_contents,
                               selection.old_contents == shared_tab_);
    SetContentsBorderVisible(selection.new_contents,
                             selection.new_contents == shared_tab_);
  }
}

void TabSharingUIViews::TabChangedAt(WebContents* contents,
                                     int index,
                                     TabChangeType change_type) {
  // Sad tab cannot be shared so don't create an infobar for it.
  auto* sad_tab_helper = SadTabHelper::FromWebContents(contents);
  if (sad_tab_helper && sad_tab_helper->sad_tab())
    return;

  if (infobars_.find(contents) == infobars_.end()) {
    CreateInfobarForWebContents(contents);
  }
}

void TabSharingUIViews::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                         bool animate) {
  auto infobars_entry = std::find_if(infobars_.begin(), infobars_.end(),
                                     [infobar](const auto& infobars_entry) {
                                       return infobars_entry.second == infobar;
                                     });
  if (infobars_entry == infobars_.end())
    return;

  infobar->owner()->RemoveObserver(this);
  infobars_.erase(infobars_entry);
  if (infobars::ContentInfoBarManager::WebContentsFromInfoBar(infobar) ==
      shared_tab_)
    StopSharing();
}

void TabSharingUIViews::DidFinishNavigation(content::NavigationHandle* handle) {
  // TODO(https://crbug.com/1218946): With MPArch there may be multiple main
  // frames. This caller was converted automatically to the primary main frame
  // to preserve its semantics. Follow up to confirm correctness.
  if (!handle->IsInPrimaryMainFrame() || !handle->HasCommitted() ||
      handle->IsSameDocument() || handle->GetWebContents() != shared_tab_) {
    return;
  }
  shared_tab_name_ = GetTabName(shared_tab_);
  for (const auto& infobars_entry : infobars_) {
    // Recreate infobars to reflect the new shared tab's hostname.
    if (infobars_entry.first != shared_tab_)
      CreateInfobarForWebContents(infobars_entry.first);
  }
}

void TabSharingUIViews::WebContentsDestroyed() {
  StopSharing();
}

void TabSharingUIViews::CreateInfobarsForAllTabs() {
  BrowserList* browser_list = BrowserList::GetInstance();
  for (auto* browser : *browser_list) {
    OnBrowserAdded(browser);

    TabStripModel* tab_strip_model = browser->tab_strip_model();
    for (int i = 0; i < tab_strip_model->count(); i++) {
      CreateInfobarForWebContents(tab_strip_model->GetWebContentsAt(i));
    }
  }
  browser_list->AddObserver(this);
}

void TabSharingUIViews::CreateInfobarForWebContents(WebContents* contents) {
  DCHECK(contents);

  auto infobars_entry = infobars_.find(contents);
  // Recreate the infobar if it already exists.
  if (infobars_entry != infobars_.end()) {
    infobars_entry->second->owner()->RemoveObserver(this);
    infobars_entry->second->RemoveSelf();
  }
  auto* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(contents);
  infobar_manager->AddObserver(this);

  const bool is_capturing_tab = (GetGlobalId(contents) == capturer_);
  const bool is_captured_tab = (contents == shared_tab_);

  // We may want to show the "Share this tab instead" button, but we can only do
  // so if we have a |source_callback_| and if this tab is neither the capturing
  // nor captured tab.
  const bool is_share_instead_button_possible =
      !source_callback_.is_null() && !is_capturing_tab && !is_captured_tab;

  // If sharing this tab instead of the currently captured tab is possible, it
  // may still be blocked by enterprise policy. If the enterprise policy is
  // active, create an observer that will inform us when it's compliance state
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

  absl::optional<TabSharingInfoBarDelegate::FocusTarget> focus_target;
  if (can_focus_capturer_) {
    // Self-capture -> no switch-to button.
    // Capturer -> switch-to-captured.
    // Captured -> switch-to-capturer.
    // Otherwise -> no switch-to button.
    if (is_capturing_tab && !is_captured_tab) {
      focus_target = {GetGlobalId(shared_tab_), TabFavicon(shared_tab_)};
      captured_favicon_hash_ = GetHash(focus_target->icon);
    } else if (!is_capturing_tab && is_captured_tab) {
      focus_target = {capturer_, TabFavicon(capturer_)};
      capturer_favicon_hash_ = GetHash(focus_target->icon);
    }
  }

  // Determine if we are currently allowed to share this tab by policy.
  const bool is_sharing_allowed_by_policy =
      !capturer_restricted_to_same_origin_ ||
      url::IsSameOriginWith(capturer_origin_,
                            contents->GetLastCommittedURL().GetOrigin());

  // Never show the [share this tab instead] if sharing is not possible or is
  // blocked by policy.
  const bool can_show_share_instead_button =
      is_share_instead_button_possible && is_sharing_allowed_by_policy;

  infobars_[contents] = TabSharingInfoBarDelegate::Create(
      infobar_manager, shared_tab_name_, app_name_,
      shared_tab_ == contents /*shared_tab*/, can_show_share_instead_button,
      focus_target, this, favicons_used_for_switch_to_tab_button_);
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
  if (!shared_tab_)
    return;

  tab_capture_indicator_ui_ = MediaCaptureDevicesDispatcher::GetInstance()
                                  ->GetMediaStreamCaptureIndicator()
                                  ->RegisterMediaStream(shared_tab_, {device});
  tab_capture_indicator_ui_->OnStarted(
      base::OnceClosure(), content::MediaStreamUI::SourceCallback(),
      /*label=*/std::string(), /*screen_capture_ids=*/{},
      content::MediaStreamUI::StateChangeCallback());
}

void TabSharingUIViews::FaviconPeriodicUpdate(size_t share_session_seq_num) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(favicons_used_for_switch_to_tab_button_);

  if (share_session_seq_num != share_session_seq_num_) {
    return;
  }

  RefreshFavicons();

  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&TabSharingUIViews::FaviconPeriodicUpdate,
                     weak_factory_.GetWeakPtr(), share_session_seq_num),
      base::Milliseconds(500));
}

void TabSharingUIViews::RefreshFavicons() {
  if (!shared_tab_) {
    return;
  }

  WebContents* const capturer = WebContentsFromId(capturer_);
  if (!capturer) {
    return;
  }

  // If the capturer's favicon has changed, update the captured tab's button.
  MaybeUpdateFavicon(capturer, &capturer_favicon_hash_, shared_tab_);

  // If the captured tab's favicon has changed, update the capturer's button.
  MaybeUpdateFavicon(shared_tab_, &captured_favicon_hash_, capturer);
}

void TabSharingUIViews::MaybeUpdateFavicon(
    WebContents* focus_target,
    absl::optional<uint32_t>* current_hash,
    WebContents* infobar_owner) {
  const ui::ImageModel favicon = TabFavicon(focus_target);
  const uint32_t hash = GetHash(favicon);
  if (*current_hash != hash) {
    *current_hash = hash;
    // TODO(crbug.com/1224363): Update favicons without recreating infobars.
    // To do so cleanly requires that |infobars_| map to |ConfirmInfoBar|.
    CreateInfobarForWebContents(infobar_owner);
  }
}

ui::ImageModel TabSharingUIViews::TabFavicon(WebContents* web_contents) const {
  if (!favicons_used_for_switch_to_tab_button_) {
    return ui::ImageModel();
  }

  if (!web_contents) {
    return ui::ImageModel::FromImage(favicon::GetDefaultFavicon());
  }

  auto it = favicon_overrides_for_testing_.find(web_contents);
  if (it != favicon_overrides_for_testing_.end()) {
    return it->second;
  }

  const gfx::Image favicon = favicon::TabFaviconFromWebContents(web_contents);
  return ui::ImageModel::FromImage(
      favicon.IsEmpty() ? favicon::GetDefaultFavicon() : favicon);
}

ui::ImageModel TabSharingUIViews::TabFavicon(
    GlobalRenderFrameHostId rfh_id) const {
  return TabFavicon(WebContentsFromId(rfh_id));
}

void TabSharingUIViews::SetTabFaviconForTesting(
    content::WebContents* web_contents,
    const ui::ImageModel& favicon) {
  favicon_overrides_for_testing_[web_contents] = favicon;
}

void TabSharingUIViews::StopCaptureDueToPolicy(content::WebContents* contents) {
  DCHECK(shared_tab_ == contents);
  StopSharing();
  // We use |contents| rather than |shared_tab_| here because |shared_tab_| is
  // cleared by the call to StopSharing().
  capture_policy::ShowCaptureTerminatedDialog(contents);
}
