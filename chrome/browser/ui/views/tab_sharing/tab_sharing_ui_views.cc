// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_sharing/tab_sharing_ui_views.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/sad_tab_helper.h"
#include "chrome/browser/ui/tab_sharing/tab_sharing_infobar_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/infobars/core/infobar.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/common/loader/network_utils.h"
#include "ui/gfx/color_palette.h"

#if defined(OS_WIN)
#include "ui/views/widget/native_widget_aura.h"
#endif

namespace {

#if !defined(OS_CHROMEOS)
const int kContentsBorderThickness = 5;
const float kContentsBorderOpacity = 0.50;
const SkColor kContentsBorderColor = gfx::kGoogleBlue500;

void InitContentsBorderWidget(content::WebContents* contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
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
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
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

void SetContentsBorderVisible(content::WebContents* contents, bool visible) {
  // TODO(https://crbug.com/1030925) fix contents border on ChromeOS.
#if !defined(OS_CHROMEOS)
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

base::string16 GetTabName(content::WebContents* tab) {
  GURL url = tab->GetLastCommittedURL();
  const base::string16 tab_name =
      blink::network_utils::IsOriginSecure(url)
          ? base::UTF8ToUTF16(net::GetHostAndOptionalPort(url))
          : url_formatter::FormatUrlForSecurityDisplay(url.GetOrigin());
  return tab_name.empty() ? tab->GetTitle() : tab_name;
}

}  // namespace

// static
std::unique_ptr<TabSharingUI> TabSharingUI::Create(
    const content::DesktopMediaID& media_id,
    base::string16 app_name) {
  return base::WrapUnique(new TabSharingUIViews(media_id, app_name));
}

TabSharingUIViews::TabSharingUIViews(const content::DesktopMediaID& media_id,
                                     base::string16 app_name)
    : shared_tab_media_id_(media_id), app_name_(std::move(app_name)) {
  shared_tab_ = content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(
          media_id.web_contents_id.render_process_id,
          media_id.web_contents_id.main_render_frame_id));
  Observe(shared_tab_);
  shared_tab_name_ = GetTabName(shared_tab_);
  profile_ = ProfileManager::GetLastUsedProfileAllowedByPolicy();
  // TODO(https://crbug.com/1030925) fix contents border on ChromeOS.
#if !defined(OS_CHROMEOS)
  InitContentsBorderWidget(shared_tab_);
#endif
}

TabSharingUIViews::~TabSharingUIViews() {
  if (!infobars_.empty())
    StopSharing();
}

gfx::NativeViewId TabSharingUIViews::OnStarted(
    base::OnceClosure stop_callback,
    content::MediaStreamUI::SourceCallback source_callback) {
  source_callback_ = std::move(source_callback);
  stop_callback_ = std::move(stop_callback);
  CreateInfobarsForAllTabs();
  SetContentsBorderVisible(shared_tab_, true);
  CreateTabCaptureIndicator();
  return 0;
}

void TabSharingUIViews::StartSharing(infobars::InfoBar* infobar) {
  if (source_callback_.is_null())
    return;

  SetContentsBorderVisible(shared_tab_, false);

  content::WebContents* shared_tab =
      InfoBarService::WebContentsFromInfoBar(infobar);
  DCHECK(shared_tab);
  DCHECK_EQ(infobars_[shared_tab], infobar);
  shared_tab_ = shared_tab;
  shared_tab_name_ = GetTabName(shared_tab_);

  content::RenderFrameHost* main_frame = shared_tab->GetMainFrame();
  DCHECK(main_frame);
  RemoveInfobarsForAllTabs();
  source_callback_.Run(content::DesktopMediaID(
      content::DesktopMediaID::TYPE_WEB_CONTENTS,
      content::DesktopMediaID::kNullId,
      content::WebContentsMediaCaptureId(main_frame->GetProcess()->GetID(),
                                         main_frame->GetRoutingID())));
}

void TabSharingUIViews::StopSharing() {
  if (!stop_callback_.is_null())
    std::move(stop_callback_).Run();
  RemoveInfobarsForAllTabs();
  SetContentsBorderVisible(shared_tab_, false);
  tab_capture_indicator_ui_.reset();
  shared_tab_ = nullptr;
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

void TabSharingUIViews::TabChangedAt(content::WebContents* contents,
                                     int index,
                                     TabChangeType change_type) {
  // Sad tab cannot be shared so don't create an infobar for it.
  auto* sad_tab_helper = SadTabHelper::FromWebContents(contents);
  if (sad_tab_helper && sad_tab_helper->sad_tab())
    return;

  if (infobars_.find(contents) == infobars_.end())
    CreateInfobarForWebContents(contents);
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
  if (InfoBarService::WebContentsFromInfoBar(infobar) == shared_tab_)
    StopSharing();
}

void TabSharingUIViews::DidFinishNavigation(content::NavigationHandle* handle) {
  // Only interested in committed navigations on the shared tab that result in
  // changing the shared tab's name.
  if (!handle->IsInMainFrame() || !handle->HasCommitted() ||
      handle->IsSameDocument() || handle->GetWebContents() != shared_tab_ ||
      GetTabName(shared_tab_) == shared_tab_name_) {
    return;
  }
  shared_tab_name_ = GetTabName(shared_tab_);
  for (const auto& infobars_entry : infobars_) {
    // Recreate infobars to reflect the new shared tab name.
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

void TabSharingUIViews::CreateInfobarForWebContents(
    content::WebContents* contents) {
  auto infobars_entry = infobars_.find(contents);
  // Recreate the infobar if it already exists.
  if (infobars_entry != infobars_.end()) {
    infobars_entry->second->owner()->RemoveObserver(this);
    infobars_entry->second->RemoveSelf();
  }
  auto* infobar_service = InfoBarService::FromWebContents(contents);
  infobar_service->AddObserver(this);
  infobars_[contents] = TabSharingInfoBarDelegate::Create(
      infobar_service, shared_tab_name_, app_name_,
      shared_tab_ == contents /*shared_tab*/,
      !source_callback_.is_null() /*can_share*/, this);
}

void TabSharingUIViews::RemoveInfobarsForAllTabs() {
  BrowserList::GetInstance()->RemoveObserver(this);
  TabStripModelObserver::StopObservingAll(this);

  for (const auto& infobars_entry : infobars_) {
    infobars_entry.second->owner()->RemoveObserver(this);
    infobars_entry.second->RemoveSelf();
  }

  infobars_.clear();
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
      base::OnceClosure(), content::MediaStreamUI::SourceCallback());
}
