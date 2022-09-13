// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/session_id.h"

#include <ostream>

#include "components/sessions/core/session_id_generator.h"

// static
SessionID SessionID::NewUnique() {
  return sessions::SessionIdGenerator::GetInstance()->NewUnique();
}

std::ostream& operator<<(std::ostream& out, SessionID id) {
  out << id.id();
  return out;
}
