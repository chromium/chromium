// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/extraction/filter_extractor.h"

#include "base/notimplemented.h"
#include "url/gurl.h"

namespace multistep_filter {

FilterExtractor::FilterExtractor() = default;
FilterExtractor::~FilterExtractor() = default;

void FilterExtractor::ExtractAnnotationFromUrl(const GURL& url) {
  // TODO(crbug.com/483670268): Implement the logic to extract annotations and
  // store them to the database.
  NOTIMPLEMENTED();
}

}  // namespace multistep_filter
