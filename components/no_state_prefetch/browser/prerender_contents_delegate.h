// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_BROWSER_PRERENDER_CONTENTS_DELEGATE_H_
#define COMPONENTS_NO_STATE_PREFETCH_BROWSER_PRERENDER_CONTENTS_DELEGATE_H_

#include "components/no_state_prefetch/common/prerender_types.mojom.h"
#include "content/public/browser/render_frame_host.h"

namespace content {
class WebContents;
}

namespace prerender {

// PrerenderContentsDelegate allows a content embedder to customize
// PrerenderContents logic.
class PrerenderContentsDelegate {
 public:
  PrerenderContentsDelegate();
  virtual ~PrerenderContentsDelegate() = default;

  // Handle creation of new PrerenderContents.
  virtual void OnPrerenderContentsCreated(content::WebContents* web_contents);

  // Prepare for |web_contents| to no longer be PrerenderContents.
  virtual void ReleasePrerenderContents(content::WebContents* web_contents);
};

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_BROWSER_PRERENDER_CONTENTS_DELEGATE_H_
