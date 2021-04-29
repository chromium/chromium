// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_CLIENT_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_CLIENT_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "url/gurl.h"

namespace history {

struct URLAndTitle {
  GURL url;
  std::u16string title;
};

class HistoryBackendClient {
 public:
  HistoryBackendClient() {}
  virtual ~HistoryBackendClient() {}

  // Returns true if the specified URL is pinned due to being bookmarked or used
  // by the password manager.
  virtual bool IsPinnedURL(const GURL& url) = 0;

  // Returns the set of pinned URLs with their titles.
  virtual std::vector<URLAndTitle> GetPinnedURLs() = 0;

  // Returns whether `url` should be considered web-safe (see
  // content::ChildProcessSecurityPolicy).
  virtual bool IsWebSafe(const GURL& url) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HistoryBackendClient);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_BACKEND_CLIENT_H_
