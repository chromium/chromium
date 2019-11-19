// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/settings/engine_settings.h"

namespace chrome_cleaner {

bool IsSupportedEngine(Engine::Name engine) {
  return engine == Engine::TEST_ONLY;
}

Engine::Name GetDefaultEngine() {
  return Engine::TEST_ONLY;
}

std::string GetEngineName(Engine::Name engine) {
  return "Test";
}

std::string GetEngineVersion(Engine::Name engine) {
  return "0.1";
}

ProcessInformation::Process GetEngineProcessType(Engine::Name engine) {
  return ProcessInformation::TEST_SANDBOX;
}

ResultCode GetEngineDisconnectionErrorCode(Engine::Name engine) {
  return RESULT_CODE_TEST_ENGINE_SANDBOX_DISCONNECTED_TOO_SOON;
}

}  // namespace chrome_cleaner
