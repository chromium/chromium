// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extensions/cast_extension_web_contents_observer.h"

namespace extensions {

CastExtensionWebContentsObserver::CastExtensionWebContentsObserver(
    content::WebContents* web_contents)
    : ExtensionWebContentsObserver(web_contents),
      content::WebContentsUserData<CastExtensionWebContentsObserver>(
          *web_contents) {}

CastExtensionWebContentsObserver::~CastExtensionWebContentsObserver() {}

void CastExtensionWebContentsObserver::CreateForWebContents(
    content::WebContents* web_contents) {
  content::WebContentsUserData<
      CastExtensionWebContentsObserver>::CreateForWebContents(web_contents);

  // Initialize this instance if necessary.
  FromWebContents(web_contents)->Initialize();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CastExtensionWebContentsObserver);

}  // namespace extensions
