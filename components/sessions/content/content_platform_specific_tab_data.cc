// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/content/content_platform_specific_tab_data.h"

#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"

namespace sessions {

ContentPlatformSpecificTabData::ContentPlatformSpecificTabData(
    content::WebContents* web_contents)
    :  // TODO(ajwong): This does not correctly handle storage for isolated
       // apps.
      session_storage_namespace_(web_contents->GetController()
                                     .GetDefaultSessionStorageNamespace()) {}

ContentPlatformSpecificTabData::ContentPlatformSpecificTabData() = default;
ContentPlatformSpecificTabData::~ContentPlatformSpecificTabData() = default;

}  // namespace sessions
