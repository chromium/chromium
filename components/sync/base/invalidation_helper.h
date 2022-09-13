// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_INVALIDATION_HELPER_H_
#define COMPONENTS_SYNC_BASE_INVALIDATION_HELPER_H_

#include "components/invalidation/public/invalidation_util.h"
#include "components/sync/base/model_type.h"

namespace syncer {

invalidation::TopicSet ModelTypeSetToTopicSet(ModelTypeSet model_types);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_INVALIDATION_HELPER_H_
