// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_CLIENT_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_CLIENT_H_

#include "base/memory/scoped_refptr.h"

namespace subresource_filter {

class SubresourceFilterClient {
 public:
  virtual ~SubresourceFilterClient() = default;

  // Informs the embedder to show some UI indicating that resources are being
  // blocked. This method will be called at most once per main-frame navigation.
  virtual void ShowNotification() = 0;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_SUBRESOURCE_FILTER_CLIENT_H_
