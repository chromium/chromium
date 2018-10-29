// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/vr_tab_helper.h"

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/web_preferences.h"
#include "device/vr/buildflags/buildflags.h"

#if defined(OS_ANDROID)
#include "base/feature_list.h"
#include "chrome/browser/android/chrome_feature_list.h"
#endif

using content::WebContents;
using content::WebPreferences;

namespace vr {

VrTabHelper::VrTabHelper(content::WebContents* contents)
    : web_contents_(contents) {}

VrTabHelper::~VrTabHelper() {}

void VrTabHelper::SetIsInVr(bool is_in_vr) {
  if (is_in_vr_ == is_in_vr)
    return;

  is_in_vr_ = is_in_vr;

  WebPreferences web_prefs =
      web_contents_->GetRenderViewHost()->GetWebkitPreferences();
  web_prefs.immersive_mode_enabled = is_in_vr_;
  web_contents_->GetRenderViewHost()->UpdateWebkitPreferences(web_prefs);
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

bool VrTabHelper::IsUiSuppressedInVr(content::WebContents* contents,
                                     UiSuppressedElement element) {
  if (!IsInVr(contents))
    return false;

  bool suppress = false;
  switch (element) {
    // The following are suppressed if in VR.
    case UiSuppressedElement::kHttpAuth:
    case UiSuppressedElement::kSslClientCertificate:
    case UiSuppressedElement::kUsbChooser:
    case UiSuppressedElement::kFileChooser:
    case UiSuppressedElement::kBluetoothChooser:
    case UiSuppressedElement::kPasswordManager:
    case UiSuppressedElement::kMediaRouterPresentationRequest:
    // Note that this enum suppresses two type of UIs. One is Chrome's missing
    // storage permission Dialog which is an Android AlertDialog. And if user
    // clicked positive button on the AlertDialog, Chrome will request storage
    // permission from Android which triggers standard permission request
    // dialog. Permission request dialog is not supported in VR either (see
    // https://crbug.com/642934). So we need to make sure that both AlertDialog
    // and permission request dialog are supported in VR before we disable this
    // suppression.
    case UiSuppressedElement::kFileAccessPermission:
    case UiSuppressedElement::kContextMenu:
      suppress = true;
      break;
    case UiSuppressedElement::kPlaceholderForPreviousHighValue:
    case UiSuppressedElement::kCount:
      suppress = false;
      NOTREACHED();
      break;
  }
  if (suppress) {
    UMA_HISTOGRAM_ENUMERATION("VR.Shell.EncounteredSuppressedUI", element,
                              UiSuppressedElement::kCount);
  }
  return suppress;
}

}  // namespace vr
