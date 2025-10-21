// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"

#include "base/memory/raw_ptr.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace optimization_guide {

MockRemoteModelExecutor::MockRemoteModelExecutor() = default;
MockRemoteModelExecutor::~MockRemoteModelExecutor() = default;

}  // namespace optimization_guide
