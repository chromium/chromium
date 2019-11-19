// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_SETTINGS_ENGINE_SETTINGS_H_
#define CHROME_CHROME_CLEANER_SETTINGS_ENGINE_SETTINGS_H_

#include <string>

#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace chrome_cleaner {

// Returns true if the engine is supported.
bool IsSupportedEngine(Engine::Name engine);

// Returns the engine to use by default if none is passed on the command-line.
Engine::Name GetDefaultEngine();

// Returns the engine's name, which is also used as the name of its sandbox
// process. This must be in a format suitable for logging in reports and crash
// keys.
std::string GetEngineName(Engine::Name engine);

// Returns string representation of the engine's version or an empty string if
// not available.
std::string GetEngineVersion(Engine::Name engine);

// Returns the type of the engine's sandbox process for logging.
ProcessInformation::Process GetEngineProcessType(Engine::Name engine);

// Returns the error code that should be logged if the connection to the
// engine's sandbox is broken.
ResultCode GetEngineDisconnectionErrorCode(Engine::Name engine);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_SETTINGS_ENGINE_SETTINGS_H_
