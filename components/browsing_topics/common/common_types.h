// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_TOPICS_COMMON_COMMON_TYPES_H_
#define COMPONENTS_BROWSING_TOPICS_COMMON_COMMON_TYPES_H_

#include "base/types/strong_alias.h"

namespace browsing_topics {

using Topic = base::StrongAlias<class TopicTag, int>;

}  // namespace browsing_topics

#endif  // COMPONENTS_BROWSING_TOPICS_COMMON_COMMON_TYPES_H_
