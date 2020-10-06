// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_METRICS_H_
#define COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_METRICS_H_

namespace shared_highlighting {

// Update corresponding |LinkGenerationError| in enums.xml.
enum LinkGenerationError {
  kIncorrectSelector,
  kNoRange,
  kNoContext,
  kContextExhausted,
  kContextLimitReached,
  kEmptySelection,

  kMaxValue = kContextLimitReached
};
}  // namespace shared_highlighting

#endif  // COMPONENTS_SHARED_HIGHLIGHTING_CORE_COMMON_SHARED_HIGHLIGHTING_METRICS_H_