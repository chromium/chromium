// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_WEB_CONTENTS_PROXY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_WEB_CONTENTS_PROXY_H_

#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}  // namespace content

namespace performance_manager {

class PerformanceManagerTabHelper;
class WebContentsProxyImpl;

// A WebContentsProxy is used to post messages out of the performance
// manager sequence that are bound for a WebContents running on the UI thread.
// The object is bound to the UI thread. A WebContentsProxy is conceputally
// equivalent to a WeakPtr<WebContents>. Copy and assignment are explicitly
// allowed for this object.
class WebContentsProxy {
 public:
  WebContentsProxy();
  WebContentsProxy(const WebContentsProxy& other);
  WebContentsProxy(WebContentsProxy&& other);
  ~WebContentsProxy();

  WebContentsProxy& operator=(const WebContentsProxy& other);
  WebContentsProxy& operator=(WebContentsProxy&& other);

  // Allows resolving this proxy to the underlying WebContents. This must only
  // be called on the UI thread.
  content::WebContents* Get() const;

  // Returns the ID of the last committed navigation in the main frame of the
  // web contents. The return value of this is only meaningful if Get()
  // returns a non-null value, otherwise it will always return the sentinel
  // value of 0 to indicate an invalid navigation ID. This must only be called
  // on the UI thread.
  int64_t LastNavigationId() const;

 protected:
  friend class PerformanceManagerTabHelper;

  explicit WebContentsProxy(const base::WeakPtr<WebContentsProxyImpl>& impl);

 private:
  base::WeakPtr<WebContentsProxyImpl> impl_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_WEB_CONTENTS_PROXY_H_
