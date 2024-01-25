// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_WEB_CONTENTS_PROXY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_WEB_CONTENTS_PROXY_H_

#include "base/memory/weak_ptr.h"

namespace content {
class WebContents;
}  // namespace content

namespace performance_manager {

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

 protected:
  friend class PerformanceManagerTabHelper;

  explicit WebContentsProxy(base::WeakPtr<content::WebContents> web_contents);

 private:
  base::WeakPtr<content::WebContents> web_contents_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_WEB_CONTENTS_PROXY_H_
