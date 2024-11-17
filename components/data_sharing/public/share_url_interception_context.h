// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_PUBLIC_SHARE_URL_INTERCEPTION_CONTEXT_H_
#define COMPONENTS_DATA_SHARING_PUBLIC_SHARE_URL_INTERCEPTION_CONTEXT_H_

namespace data_sharing {

// Base context for URL interceptions. Platforms can subclass this to pass
// additional context such as a browser window.
struct ShareURLInterceptionContext {
  virtual ~ShareURLInterceptionContext();
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_PUBLIC_SHARE_URL_INTERCEPTION_CONTEXT_H_
