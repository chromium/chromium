// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_WEB_WRAPPER_H_
#define COMPONENTS_COMMERCE_CORE_WEB_WRAPPER_H_

#include <string>

#include "base/callback.h"
#include "url/gurl.h"

namespace base {
class Value;
}  // namespace base

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

  // Execute the provided |script| and pass the result through |callback|. This
  // will run in an isolated world if possible.
  virtual void RunJavascript(
      const std::u16string& script,
      base::OnceCallback<void(const base::Value)> callback) = 0;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_WEB_WRAPPER_H_
