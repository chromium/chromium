// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_SESSION_ID_H_
#define COMPONENTS_SESSIONS_CORE_SESSION_ID_H_

#include <stdint.h>
#include <functional>
#include <iosfwd>

#include "components/sessions/core/sessions_export.h"

// Uniquely identifies a tab or window for the duration of a session. Pass by
// value.
class SESSIONS_EXPORT SessionID {
 public:
  typedef int32_t id_type;

  // Creates a new instance representing an ID that has never been used before
  // locally (even across browser restarts).
  static SessionID NewUnique();

  // Special value representing a null-like, invalid ID. It's the only value
  // that returns false for is_valid().
  static constexpr SessionID InvalidValue() { return SessionID(-1); }

  // Should be used rarely because it can lead to collisions.
  static constexpr SessionID FromSerializedValue(id_type value) {
    return IsValidValue(value) ? SessionID(value) : InvalidValue();
  }

  // Returns whether a certain underlying ID value would represent a valid
  // SessionID instance. Note that zero is also considered invalid.
  static constexpr bool IsValidValue(id_type value) { return value > 0; }

  // All IDs are valid except InvalidValue() above.
  bool is_valid() const { return IsValidValue(id_); }

  // Returns the underlying id.
  id_type id() const { return id_; }

  struct Hasher {
    inline std::size_t operator()(SessionID id) const { return id.id(); }
  };

 private:
  explicit constexpr SessionID(id_type id) : id_(id) {}

  id_type id_;
};

inline bool operator==(SessionID lhs, SessionID rhs) {
  return lhs.id() == rhs.id();
}

inline bool operator!=(SessionID lhs, SessionID rhs) {
  return lhs.id() != rhs.id();
}

inline bool operator<(SessionID lhs, SessionID rhs) {
  return lhs.id() < rhs.id();
}

// For use in gtest-based unit tests.
SESSIONS_EXPORT std::ostream& operator<<(std::ostream& out, SessionID id);

#endif  // COMPONENTS_SESSIONS_CORE_SESSION_ID_H_
