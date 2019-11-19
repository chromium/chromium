// Copyright 2006 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/pickle.h"
#include "components/sessions/core/session_command.h"

namespace sessions {

SessionCommand::SessionCommand(id_type id, size_type size)
    : id_(id),
      contents_(size, 0) {
}

SessionCommand::SessionCommand(id_type id, const base::Pickle& pickle)
    : id_(id),
      contents_(pickle.size(), 0) {
  DCHECK(pickle.size() < std::numeric_limits<size_type>::max());
  memcpy(contents(), pickle.data(), pickle.size());
}

bool SessionCommand::GetPayload(void* dest, size_t count) const {
  if (size() != count)
    return false;
  memcpy(dest, &(contents_[0]), count);
  return true;
}

std::unique_ptr<base::Pickle> SessionCommand::PayloadAsPickle() const {
  return std::make_unique<base::Pickle>(contents(), static_cast<int>(size()));
}

}  // namespace sessions
