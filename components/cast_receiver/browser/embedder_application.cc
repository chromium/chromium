// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/public/embedder_application.h"

namespace cast_receiver {

EmbedderApplication::~EmbedderApplication() = default;

std::ostream& operator<<(std::ostream& os,
                         EmbedderApplication::ApplicationStopReason reason) {
  switch (reason) {
    case EmbedderApplication::ApplicationStopReason::kUndefined:
      return os << "Undefined";
    case EmbedderApplication::ApplicationStopReason::kApplicationRequest:
      return os << "Application Request";
    case EmbedderApplication::ApplicationStopReason::kIdleTimeout:
      return os << "Idle Timeout";
    case EmbedderApplication::ApplicationStopReason::kUserRequest:
      return os << "Use Request";
    case EmbedderApplication::ApplicationStopReason::kHttpError:
      return os << "HTTP Error";
    case EmbedderApplication::ApplicationStopReason::kRuntimeError:
      return os << "Runtime Error";
  }
}

}  // namespace cast_receiver
