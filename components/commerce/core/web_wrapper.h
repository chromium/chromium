// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_WEB_WRAPPER_H_
#define COMPONENTS_COMMERCE_CORE_WEB_WRAPPER_H_

#include "url/gurl.h"

namespace commerce {

// A wrapper class for WebContent on desktop and android or WebState on iOS.
class WebWrapper {
 public:
  WebWrapper() = default;
  WebWrapper(const WebWrapper&) = delete;
  virtual ~WebWrapper() = default;

  // Get the URL that is currently being displayed for the page.
  virtual const GURL& GetLastCommittedURL() = 0;

  // Whether content is off the record or in incognito mode.
  virtual bool IsOffTheRecord() = 0;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_WEB_WRAPPER_H_
