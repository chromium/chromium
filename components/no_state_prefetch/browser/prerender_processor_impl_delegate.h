// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_BROWSER_PRERENDER_PROCESSOR_IMPL_DELEGATE_H_
#define COMPONENTS_NO_STATE_PREFETCH_BROWSER_PRERENDER_PROCESSOR_IMPL_DELEGATE_H_

namespace content {
class BrowserContext;
}

namespace prerender {

class NoStatePrefetchLinkManager;

class PrerenderProcessorImplDelegate {
 public:
  virtual ~PrerenderProcessorImplDelegate() = default;

  // Gets the NoStatePrefetchLinkManager associated with |browser_context|.
  virtual NoStatePrefetchLinkManager* GetNoStatePrefetchLinkManager(
      content::BrowserContext* browser_context) = 0;
};

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_BROWSER_PRERENDER_PROCESSOR_IMPL_DELEGATE_H_
