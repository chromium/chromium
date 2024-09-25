// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_COMMON_UTILS_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_COMMON_UTILS_H_

class GURL;

namespace subresource_filter {

// Child frame navigations and initial root frame navigations matching these
// URLs/ schemes will not trigger ReadyToCommitNavigation in the browser
// process, so they must be treated specially to maintain activation. Each
// should inherit the activation of its parent in the case of a child frame and
// its opener in the case of a root frame. This also accounts for the ability of
// the parent/opener to affect the frame's content more directly, e.g. through
// document.write(), even though these URLs won't match a filter list rule by
// themselves.
bool ShouldInheritActivation(const GURL& url);

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_SHARED_COMMON_UTILS_H_
