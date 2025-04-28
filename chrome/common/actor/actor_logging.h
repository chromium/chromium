// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ACTOR_ACTOR_LOGGING_H_
#define CHROME_COMMON_ACTOR_ACTOR_LOGGING_H_

#include "base/logging.h"

// Logging utility for actor framework. See chrome/{browser|renderer}/actor for
// details.

#define ACTOR_LOG() VLOG(1) << "[ActorTool]: "

#endif  // CHROME_COMMON_ACTOR_ACTOR_LOGGING_H_
