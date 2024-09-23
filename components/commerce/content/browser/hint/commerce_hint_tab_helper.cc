// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/content/browser/hint/commerce_hint_tab_helper.h"

namespace commerce_hint {

CommerceHintTabHelper::CommerceHintTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<CommerceHintTabHelper>(*web_contents) {}

CommerceHintTabHelper::~CommerceHintTabHelper() = default;

void CommerceHintTabHelper::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  if (!render_frame_host->IsInPrimaryMainFrame())
    return;
  // TODO(crbug.com/40219258): Hook up with ShoppingPrompt.
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CommerceHintTabHelper);

}  // namespace commerce_hint
