// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_annotator_features.h"

namespace accessibility_annotator::features {

BASE_FEATURE(kContentAnnotator, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(int,
                   kContentAnnotatorMaxCacheAnnotations,
                   &kContentAnnotator,
                   "content_annotator_max_cache_annotations",
                   10);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kContentAnnotatorConfirmedStatusLookbackWindow,
                   &kContentAnnotator,
                   "content_annotator_confirmed_status_lookback_window",
                   base::Minutes(20));
BASE_FEATURE_PARAM(bool,
                   kContentAnnotatorEnableMultiTabAnnotations,
                   &kContentAnnotator,
                   "content_annotator_enable_multi_tab_annotations",
                   false);

BASE_FEATURE(kAccessibilityAnnotator, base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/484049558): Remove this feature once the SQLite database
// storage is ready with the initial schema as default storage.
// Enables the accessibility annotator database storage. This will allow the
// accessibility annotator backend to create the SQLite database.
BASE_FEATURE(kAccessibilityAnnotatorDatabaseStorage,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace accessibility_annotator::features
