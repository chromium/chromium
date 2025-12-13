// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_WEB_STATE_WRAPPER_H_
#define COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_WEB_STATE_WRAPPER_H_

#include "base/time/time.h"
#include "url/gurl.h"

namespace page_content_annotations {

// Indicates whether the tab or webcontents is visible to the user, including
// partially.
enum class PageContentVisibility {
  kVisible,
  kHidden,
};

// A wrapper around WebContents and WebState to be usable by all platforms.
struct WebStateWrapper {
  WebStateWrapper(bool is_off_the_record,
                  const GURL& last_committed_url,
                  const base::Time& navigation_timestamp,
                  PageContentVisibility visibility);
  ~WebStateWrapper();

  WebStateWrapper(const WebStateWrapper&) = delete;
  WebStateWrapper& operator=(const WebStateWrapper&) = delete;

  const bool is_off_the_record;
  const GURL last_committed_url;
  const base::Time navigation_timestamp;
  const PageContentVisibility visibility;
};

}  // namespace page_content_annotations

#endif  // COMPONENTS_PAGE_CONTENT_ANNOTATIONS_CORE_WEB_STATE_WRAPPER_H_
