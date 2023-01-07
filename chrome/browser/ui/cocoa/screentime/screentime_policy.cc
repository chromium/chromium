// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/screentime/screentime_policy.h"

#include "url/origin.h"

namespace screentime {

GURL URLForReporting(const GURL& url) {
  // Strip the username, password, path, and query components:
  // https://crbug.com/1188351.
  return url::Origin::Create(url).GetURL();
}

}  // namespace screentime
