// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_BROWSER_THREAD_GUARD_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_BROWSER_THREAD_GUARD_H_

namespace enterprise_connectors {

class BrowserThreadGuard {
 public:
  virtual void AssertCalledOnUIThread() = 0;
  virtual ~BrowserThreadGuard() = default;
};

}  // namespace enterprise_connectors

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CLOUD_CONTENT_SCANNING_BROWSER_THREAD_GUARD_H_
