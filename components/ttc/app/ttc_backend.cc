// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ttc/app/ttc_backend.h"

namespace ttc {

std::optional<std::string> TtcBackend::GetSessionId() const {
  return std::nullopt;
}

}  // namespace ttc
