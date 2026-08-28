// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_UTILS_H_
#define COMPONENTS_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_UTILS_H_

class GURL;

namespace contextual_cueing {

// Returns true if `url` corresponds to a root homepage or common landing page
// path (e.g., "/", "/index.html", "/default.aspx", "/welcome", etc.).
bool IsHomepageUrl(const GURL& url);

}  // namespace contextual_cueing

#endif  // COMPONENTS_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_UTILS_H_
