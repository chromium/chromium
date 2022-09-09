// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_search/unified_side_search_helper.h"

#include "chrome/browser/ui/views/side_search/unified_side_search_controller.h"

void CreateUnifiedSideSearchController(SideSearchTabContentsHelper* creator,
                                       content::WebContents* web_contents) {
  UnifiedSideSearchController::CreateForWebContents(web_contents);
  creator->SetDelegate(
      UnifiedSideSearchController::FromWebContents(web_contents)->GetWeakPtr());
}
