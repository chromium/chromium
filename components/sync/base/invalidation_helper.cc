// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/invalidation_helper.h"

#include <string>

#include "base/logging.h"

namespace syncer {

invalidation::TopicSet ModelTypeSetToTopicSet(ModelTypeSet model_types) {
  invalidation::TopicSet topics;
  for (ModelType type : model_types) {
    invalidation::Topic topic;
    if (!RealModelTypeToNotificationType(type, &topic)) {
      DLOG(WARNING) << "Invalid model type " << type;
      continue;
    }
    topics.insert(topic);
  }
  return topics;
}

}  // namespace syncer
