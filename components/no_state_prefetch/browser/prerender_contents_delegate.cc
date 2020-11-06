// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/no_state_prefetch/browser/prerender_contents_delegate.h"

namespace prerender {

PrerenderContentsDelegate::PrerenderContentsDelegate() = default;

void PrerenderContentsDelegate::OnPrerenderContentsCreated(
    content::WebContents* web_contents) {}

void PrerenderContentsDelegate::ReleasePrerenderContents(
    content::WebContents* web_contents) {}

}  // namespace prerender