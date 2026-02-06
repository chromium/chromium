// Copyright 2006 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/session_command.h"

#include <algorithm>
#include <limits>
#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/pickle.h"

namespace sessions {

SessionCommand::SessionCommand(id_type id, size_type size)
    : id_(id),
      contents_(size, 0) {
}

SessionCommand::SessionCommand(id_type id, const base::Pickle& pickle)
    : id_(id),
      contents_(pickle.size(), 0) {
  DCHECK(pickle.size() < std::numeric_limits<size_type>::max());
  contents().copy_from(pickle);
}

SessionCommand::size_type SessionCommand::GetSerializedSize() const {
  const size_type additional_overhead = sizeof(id_type);
  return std::min(size(),
                  static_cast<size_type>(std::numeric_limits<size_type>::max() -
                                         additional_overhead)) +
         additional_overhead;
}

bool SessionCommand::GetPayload(void* dest, size_t count) const {
  if (size() != count)
    return false;
  UNSAFE_TODO(memcpy(dest, &(contents_[0]), count));
  return true;
}

base::PickleIterator SessionCommand::PayloadAsPickle() const {
  return base::PickleIterator::WithData(contents());
}

}  // namespace sessions
