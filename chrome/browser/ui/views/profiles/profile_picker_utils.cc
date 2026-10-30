// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_utils.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/mojom/window_open_disposition.mojom-shared.h"
#include "ui/base/page_transition_types.h"

void OpenLearnMorePopup(Profile* profile,
                        std::unique_ptr<content::WebContents> contents,
                        const GURL& target_url,
                        const blink::mojom::WindowFeatures& window_features) {
  NavigateParams params(profile, target_url, ui::PAGE_TRANSITION_LINK);
  params.disposition = ui::mojom::WindowOpenDisposition::kNewPopup;
  params.contents_to_insert = std::move(contents);
  params.window_features = window_features;
  Navigate(&params);
}
