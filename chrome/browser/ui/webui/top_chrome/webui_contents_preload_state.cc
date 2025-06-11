// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_state.h"

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebUIContentsPreloadState);

WebUIContentsPreloadState::WebUIContentsPreloadState(content::WebContents* web_contents)
      : WebContentsUserData(*web_contents) {}
