// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_MEMORY_QUOTA_CONSUMER_H_
#define COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_MEMORY_QUOTA_CONSUMER_H_

namespace web_package {

// This class is used to check the memory quota while loading subresource
// Web Bundles. The allocated quota is released in the destructor.
class WebBundleMemoryQuotaConsumer {
 public:
  virtual ~WebBundleMemoryQuotaConsumer() = default;
  virtual bool AllocateMemory(uint64_t num_bytes) = 0;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_WEB_BUNDLE_MEMORY_QUOTA_CONSUMER_H_
