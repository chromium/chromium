// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_navigator.h"

#include "base/notimplemented.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_navigator_params_utils.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"

base::WeakPtr<content::NavigationHandle> Navigate(NavigateParams* params) {
  // PRE-CHECKS
  // TODO (crbug.com/441594986) Confirm this is correct.
  DCHECK(params->browser_window_interface);
  DCHECK(!params->contents_to_insert);
  DCHECK(!params->switch_to_singleton_tab);

  BrowserWindowInterface* source_browser = params->browser_window_interface;
  params->initiating_profile = source_browser->GetProfile();
  if (params->initiating_profile->ShutdownStarted()) {
    // Don't navigate when the profile is shutting down.
    return nullptr;
  }
  DCHECK(params->initiating_profile);

  // HANDLE DISPOSITIONS
  switch (params->disposition) {
    case WindowOpenDisposition::CURRENT_TAB: {
      if (!params->source_contents) {
        return nullptr;
      }

      content::NavigationController::LoadURLParams load_url_params =
          LoadURLParamsFromNavigateParams(params);
      return params->source_contents->GetController().LoadURLWithParams(
          load_url_params);
    }

    default:
      NOTIMPLEMENTED();
  }

  return nullptr;
}
