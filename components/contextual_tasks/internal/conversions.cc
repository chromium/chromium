// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_tasks/internal/conversions.h"

namespace contextual_tasks {

ThreadType ToThreadType(sync_pb::AiThreadSpecifics::ThreadType proto_type) {
  switch (proto_type) {
    case sync_pb::AiThreadSpecifics::UNKNOWN:
      return ThreadType::kUnknown;
    case sync_pb::AiThreadSpecifics::AI_MODE:
      return ThreadType::kAiMode;
  }
}

}  // namespace contextual_tasks
