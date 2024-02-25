// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/vr_tab_helper.h"

#include "build/build_config.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/xr_runtime_manager.h"
#include "device/vr/buildflags/buildflags.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#endif

using blink::web_pref::WebPreferences;
using content::WebContents;

namespace vr {

VrTabHelper::VrTabHelper(content::WebContents* contents)
    : content::WebContentsUserData<VrTabHelper>(*contents) {}

VrTabHelper::~VrTabHelper() {}

void VrTabHelper::SetIsInVr(bool is_in_vr) {
  if (is_in_vr_ == is_in_vr)
    return;

  is_in_vr_ = is_in_vr;

  blink::web_pref::WebPreferences web_prefs =
      GetWebContents().GetOrCreateWebPreferences();
  web_prefs.immersive_mode_enabled = is_in_vr_;
  GetWebContents().SetWebPreferences(web_prefs);
}

/* static */
bool VrTabHelper::IsInVr(content::WebContents* contents) {
  if (!contents)
    return false;

  VrTabHelper* vr_tab_helper = VrTabHelper::FromWebContents(contents);
  if (!vr_tab_helper) {
    // This can only happen for unittests.
    VrTabHelper::CreateForWebContents(contents);
    vr_tab_helper = VrTabHelper::FromWebContents(contents);
  }
  return vr_tab_helper->is_in_vr();
}

/* static */
bool VrTabHelper::IsContentDisplayedInHeadset(content::WebContents* contents) {
  if (!contents)
    return false;

  VrTabHelper* vr_tab_helper = VrTabHelper::FromWebContents(contents);
  if (!vr_tab_helper) {
    VrTabHelper::CreateForWebContents(contents);
    vr_tab_helper = VrTabHelper::FromWebContents(contents);
  }
  return vr_tab_helper->is_content_displayed_in_headset();
}

/* static */
void VrTabHelper::SetIsContentDisplayedInHeadset(content::WebContents* contents,
                                                 bool state) {
  if (!contents)
    return;
  VrTabHelper* vr_tab_helper = VrTabHelper::FromWebContents(contents);
  if (!vr_tab_helper) {
    VrTabHelper::CreateForWebContents(contents);
    vr_tab_helper = VrTabHelper::FromWebContents(contents);
  }
  bool old_state = vr_tab_helper->IsContentDisplayedInHeadset(contents);
  vr_tab_helper->SetIsContentDisplayedInHeadset(state);
  if (old_state != state) {
#if !BUILDFLAG(IS_ANDROID)
    Browser* browser = chrome::FindBrowserWithTab(contents);
    if (browser) {
      TabStripModel* tab_strip_model = browser->tab_strip_model();
      if (tab_strip_model) {
        tab_strip_model->UpdateWebContentsStateAt(
            tab_strip_model->GetIndexOfWebContents(contents),
            TabChangeType::kAll);
      }
    }
#endif
  }
}

/* static */
void VrTabHelper::ExitVrPresentation() {
#if BUILDFLAG(IS_WIN) && BUILDFLAG(ENABLE_VR)
  content::XRRuntimeManager::ExitImmersivePresentation();
#endif
}

void VrTabHelper::SetIsContentDisplayedInHeadset(bool state) {
  is_content_displayed_in_headset_ = state;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(VrTabHelper);

}  // namespace vr
