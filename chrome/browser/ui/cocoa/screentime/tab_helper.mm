// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/command_line.h"
#include "chrome/browser/ui/cocoa/screentime/fake_webpage_controller.h"
#include "chrome/browser/ui/cocoa/screentime/tab_helper.h"
#include "chrome/browser/ui/cocoa/screentime/webpage_controller.h"
#include "chrome/browser/ui/cocoa/screentime/webpage_controller_impl.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace screentime {

namespace {
bool g_use_fake_webpage_controller = false;
}

// static
void TabHelper::UseFakeWebpageControllerForTesting() {
  g_use_fake_webpage_controller = true;
}

// static
bool TabHelper::IsScreentimeEnabled() {
  static constexpr base::Feature kScreenTime{
      "ScreenTime",
      base::FEATURE_DISABLED_BY_DEFAULT,
  };
  return base::FeatureList::IsEnabled(kScreenTime);
}

TabHelper::TabHelper(content::WebContents* contents)
    : WebContentsObserver(contents), page_controller_(MakeWebpageController()) {
  NSView* contents_view = contents->GetNativeView().GetNativeNSView();
  [contents_view addSubview:page_controller_->GetView()];
}

TabHelper::~TabHelper() = default;

void TabHelper::DidFinishNavigation(content::NavigationHandle* handle) {
  // TODO(ellyjones): Some defensive programming around chrome:// URLs would
  // probably be a good idea here. It's not unimaginable that ScreenTime would
  // misbehave and end up occluding those URLs, which would be very bad.
  if (handle->IsInMainFrame() && handle->HasCommitted())
    page_controller_->PageURLChangedTo(handle->GetURL());
}

std::unique_ptr<WebpageController> TabHelper::MakeWebpageController() {
  const bool use_fake =
      g_use_fake_webpage_controller ||
      base::CommandLine::ForCurrentProcess()->HasSwitch("fake-screentime");

  // The callback is owned by the WebpageController instance, which is in turn
  // owned by this object, so it can't outlive us.
  auto callback =
      base::BindRepeating(&TabHelper::OnBlockedChanged, base::Unretained(this));
  std::unique_ptr<WebpageController> controller;
  if (use_fake)
    controller = std::make_unique<FakeWebpageController>(callback);
  else
    controller = std::make_unique<WebpageControllerImpl>(callback);
  return controller;
}

void TabHelper::OnBlockedChanged(bool blocked) {
  // TODO: Pause/resume playing media, update occlusion state on the
  // WebContents, and so on. Getting this behavior right will probably require
  // some care.
  NOTIMPLEMENTED();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabHelper)

}  // namespace screentime
