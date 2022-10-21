// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/cast_core/runtime/browser/runtime_application_dispatcher.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_service.h"
#include "third_party/cast_core/public/src/proto/common/application_config.pb.h"

namespace chromecast {

RuntimeApplicationDispatcher::~RuntimeApplicationDispatcher() = default;

}  // namespace chromecast
