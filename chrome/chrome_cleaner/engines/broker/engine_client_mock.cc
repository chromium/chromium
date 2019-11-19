// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/broker/engine_client_mock.h"

#include "base/bind_helpers.h"
#include "base/test/null_task_runner.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"

namespace chrome_cleaner {

// We don't use the mojo task runner during the mock tests, but EngineClient
// constructor still needs a valid mojo task runner.
MockEngineClient::MockEngineClient()
    : EngineClient(Engine::UNKNOWN,
                   /*logging_callback=*/base::NullCallback(),
                   /*connection_error_callback=*/base::NullCallback(),
                   MojoTaskRunner::Create()) {}

MockEngineClient::~MockEngineClient() = default;

StrictMockEngineClient::StrictMockEngineClient() = default;

StrictMockEngineClient::~StrictMockEngineClient() = default;

}  // namespace chrome_cleaner
