// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_tab_helper.h"

#include <memory>
#include <utility>

#include "chrome/browser/usb/web_usb_service_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/usb/web_usb_chooser_android.h"
#else
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/usb/web_usb_chooser_desktop.h"
#endif  // defined(OS_ANDROID)

using content::RenderFrameHost;
using content::WebContents;

namespace {

// The renderer performs its own feature policy checks so a request that gets
// to the browser process indicates malicous code.
const char kFeaturePolicyViolation[] =
    "Feature policy blocks access to WebUSB.";

}  // namespace

struct FrameUsbServices {
  std::unique_ptr<WebUsbChooser> usb_chooser;
  std::unique_ptr<WebUsbServiceImpl> web_usb_service;
  int device_connection_count_ = 0;
};

// static
UsbTabHelper* UsbTabHelper::GetOrCreateForWebContents(
    WebContents* web_contents) {
  UsbTabHelper* tab_helper = FromWebContents(web_contents);
  if (!tab_helper) {
    CreateForWebContents(web_contents);
    tab_helper = FromWebContents(web_contents);
  }
  return tab_helper;
}

UsbTabHelper::~UsbTabHelper() {}

void UsbTabHelper::CreateWebUsbService(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebUsbService> receiver) {
  if (!AllowedByFeaturePolicy(render_frame_host)) {
    mojo::ReportBadMessage(kFeaturePolicyViolation);
    return;
  }

  FrameUsbServices* frame_usb_services = GetFrameUsbService(render_frame_host);
  if (!frame_usb_services->web_usb_service) {
    frame_usb_services->web_usb_service.reset(new WebUsbServiceImpl(
        render_frame_host, GetUsbChooser(render_frame_host)));
  }
  frame_usb_services->web_usb_service->BindReceiver(std::move(receiver));
}

void UsbTabHelper::IncrementConnectionCount(
    RenderFrameHost* render_frame_host) {
  auto it = frame_usb_services_.find(render_frame_host);
  DCHECK(it != frame_usb_services_.end());
  it->second->device_connection_count_++;
  NotifyTabStateChanged();
}

void UsbTabHelper::DecrementConnectionCount(
    RenderFrameHost* render_frame_host) {
  auto it = frame_usb_services_.find(render_frame_host);
  DCHECK(it != frame_usb_services_.end());
  DCHECK_GT(it->second->device_connection_count_, 0);
  it->second->device_connection_count_--;
  NotifyTabStateChanged();
}

bool UsbTabHelper::IsDeviceConnected() const {
  for (const auto& map_entry : frame_usb_services_) {
    if (map_entry.second->device_connection_count_ > 0)
      return true;
  }
  return false;
}

UsbTabHelper::UsbTabHelper(WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

void UsbTabHelper::RenderFrameDeleted(RenderFrameHost* render_frame_host) {
  // This method handles the simple case of a frame closing.
  DeleteFrameServices(render_frame_host);
}

void UsbTabHelper::RenderFrameHostChanged(RenderFrameHost* old_host,
                                          RenderFrameHost* new_host) {
  // This method handles the case where a frame swaps its RenderFrameHost for a
  // new one on navigation.
  DeleteFrameServices(old_host);
}

void UsbTabHelper::DidFinishNavigation(content::NavigationHandle* handle) {
  // This method handles the case where a frame navigates without swapping its
  // RenderFrameHost for a new one.
  if (handle->HasCommitted() && !handle->IsSameDocument())
    DeleteFrameServices(handle->GetRenderFrameHost());
}

FrameUsbServices* UsbTabHelper::GetFrameUsbService(
    RenderFrameHost* render_frame_host) {
  FrameUsbServicesMap::const_iterator it =
      frame_usb_services_.find(render_frame_host);
  if (it == frame_usb_services_.end()) {
    std::unique_ptr<FrameUsbServices> frame_usb_services(
        new FrameUsbServices());
    it = (frame_usb_services_.insert(
              std::make_pair(render_frame_host, std::move(frame_usb_services))))
             .first;
  }
  return it->second.get();
}

void UsbTabHelper::DeleteFrameServices(RenderFrameHost* render_frame_host) {
  frame_usb_services_.erase(render_frame_host);
  NotifyTabStateChanged();
}

base::WeakPtr<WebUsbChooser> UsbTabHelper::GetUsbChooser(
    RenderFrameHost* render_frame_host) {
  FrameUsbServices* frame_usb_services = GetFrameUsbService(render_frame_host);
  if (!frame_usb_services->usb_chooser) {
    frame_usb_services->usb_chooser.reset(
#if defined(OS_ANDROID)
        new WebUsbChooserAndroid(render_frame_host));
#else
        new WebUsbChooserDesktop(render_frame_host));
#endif  // defined(OS_ANDROID)
  }
  return frame_usb_services->usb_chooser->GetWeakPtr();
}

void UsbTabHelper::NotifyTabStateChanged() const {
  // TODO(https://crbug.com/601627): Implement tab indicator for Android.
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (browser) {
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    tab_strip_model->UpdateWebContentsStateAt(
        tab_strip_model->GetIndexOfWebContents(web_contents()),
        TabChangeType::kAll);
  }
#endif
}

bool UsbTabHelper::AllowedByFeaturePolicy(
    RenderFrameHost* render_frame_host) const {
  DCHECK(WebContents::FromRenderFrameHost(render_frame_host) == web_contents());
  return render_frame_host->IsFeatureEnabled(
      blink::mojom::FeaturePolicyFeature::kUsb);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(UsbTabHelper)
