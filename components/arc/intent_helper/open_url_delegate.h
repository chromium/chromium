// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_OPEN_URL_DELEGATE_H_
#define COMPONENTS_ARC_INTENT_HELPER_OPEN_URL_DELEGATE_H_

class GURL;

namespace arc {

class OpenUrlDelegate {
 public:
  virtual ~OpenUrlDelegate() = default;

  // Opens the given URL in the Chrome browser.
  virtual void OpenUrlFromArc(const GURL& url) = 0;

  // Opens the given URL as a web app in the Chrome browser, falling back to
  // opening as a tab if no installed web app is found.
  virtual void OpenWebAppFromArc(const GURL& url) = 0;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_OPEN_URL_DELEGATE_H_
