// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/cocoa/screentime/fake_webpage_controller.h"
#include "chrome/browser/ui/cocoa/screentime/screentime_features.h"
#include "chrome/browser/ui/cocoa/screentime/screentime_policy.h"
#include "chrome/browser/ui/cocoa/screentime/tab_helper.h"
#include "chrome/browser/ui/cocoa/screentime/webpage_controller.h"
#include "chrome/browser/ui/cocoa/screentime/webpage_controller_impl.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/media_session.h"
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
bool TabHelper::IsScreentimeEnabledForProfile(Profile* profile) {
  if (profile->IsOffTheRecord())
    return false;
  if (!profile->GetPrefs()
           ->FindPreference(policy::policy_prefs::kScreenTimeEnabled)
           ->GetValue()
           ->GetBool()) {
    return false;
  }

  return IsScreenTimeEnabled();
}

TabHelper::TabHelper(content::WebContents* contents)
    : WebContentsObserver(contents),
      content::WebContentsUserData<TabHelper>(*contents),
      page_controller_(MakeWebpageController()) {
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  // Absolutely ensure that we never record a navigation for an OTR profile.
  CHECK(!profile->IsOffTheRecord());

  NSView* contents_view = contents->GetNativeView().GetNativeNSView();
  [contents_view addSubview:page_controller_->GetView()];
}

TabHelper::~TabHelper() = default;

void TabHelper::PrimaryPageChanged(content::Page& page) {
  content::RenderFrameHost& rfh = page.GetMainDocument();
  const GURL& url = rfh.GetLastCommittedURL();
  if (!url.SchemeIsHTTPOrHTTPS())
    return;
  page_controller_->PageURLChangedTo(URLForReporting(url));
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
  if (@available(macOS 12.1, *)) {
    if (use_fake) {
      controller = std::make_unique<FakeWebpageController>(callback);
    } else {
      controller = std::make_unique<WebpageControllerImpl>(callback);
    }
  } else {
    DCHECK(use_fake);
    controller = std::make_unique<FakeWebpageController>(callback);
  }
  return controller;
}

void TabHelper::OnBlockedChanged(bool blocked) {
  // TODO: Update occlusion state on the WebContents, and so on.
  // Getting this behavior right will probably require some care.
  auto* media_session = content::MediaSession::Get(web_contents());
  if (blocked)
    media_session->Suspend(content::MediaSession::SuspendType::kSystem);
  else
    media_session->Resume(content::MediaSession::SuspendType::kSystem);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabHelper);

}  // namespace screentime
