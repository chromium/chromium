// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/performance_controls/test_support/discard_mock_navigation_handle.h"

void DiscardMockNavigationHandle::SetWasDiscarded(bool was_discarded) {
  was_discarded_ = was_discarded;
}
bool DiscardMockNavigationHandle::ExistingDocumentWasDiscarded() const {
  return was_discarded_;
}
void DiscardMockNavigationHandle::SetWebContents(
    content::WebContents* web_contents) {
  web_contents_ = web_contents;
}
content::WebContents* DiscardMockNavigationHandle::GetWebContents() {
  return web_contents_;
}
