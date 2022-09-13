// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_WEB_CONTENTS_PROXY_IMPL_H_
#define COMPONENTS_PERFORMANCE_MANAGER_WEB_CONTENTS_PROXY_IMPL_H_

#include <cstdint>

namespace content {
class WebContents;
}  // namespace content

namespace performance_manager {

// A WebContentsProxyImpl is used to post messages out of the performance
// manager sequence that are bound for a WebContents running on the UI thread.
// The object is bound to the UI thread. A WeakPtr<WebContentsProxyImpl> is
// effectively equivalent to a WeakPtr<WebContents>.
//
// This class is opaquely embedded in a WebContentsProxy, which hides the fact
// that a weak pointer is being used under the hood.
class WebContentsProxyImpl {
 public:
  WebContentsProxyImpl();

  WebContentsProxyImpl(const WebContentsProxyImpl&) = delete;
  WebContentsProxyImpl& operator=(const WebContentsProxyImpl&) = delete;

  virtual ~WebContentsProxyImpl();

  // Allows resolving this proxy to the underlying WebContents. This must only
  // be called on the UI thread.
  virtual content::WebContents* GetWebContents() const = 0;

  // Returns the ID of the last committed navigation in the main frame of the
  // web contents. This must only be called on the UI thread.
  virtual int64_t LastNavigationId() const = 0;

  // Similar to the above, but for the last non same-document navigation
  // associated with this WebContents. This is always for a navigation that is
  // older or equal to "LastNavigationId". This must only be called on the UI
  // thread.
  virtual int64_t LastNewDocNavigationId() const = 0;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_WEB_CONTENTS_PROXY_IMPL_H_
