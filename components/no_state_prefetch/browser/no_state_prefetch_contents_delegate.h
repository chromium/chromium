// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_CONTENTS_DELEGATE_H_
#define COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_CONTENTS_DELEGATE_H_

namespace content {
class WebContents;
}

namespace prerender {

// NoStatePrefetchContentsDelegate allows a content embedder to customize
// NoStatePrefetchContents logic.
class NoStatePrefetchContentsDelegate {
 public:
  NoStatePrefetchContentsDelegate();
  virtual ~NoStatePrefetchContentsDelegate() = default;

  // Handle creation of new NoStatePrefetchContents.
  virtual void OnNoStatePrefetchContentsCreated(
      content::WebContents* web_contents);

  // Prepare for |web_contents| to no longer be NoStatePrefetchContents.
  virtual void ReleaseNoStatePrefetchContents(
      content::WebContents* web_contents);
};

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_BROWSER_NO_STATE_PREFETCH_CONTENTS_DELEGATE_H_
