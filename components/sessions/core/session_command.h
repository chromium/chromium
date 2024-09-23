// Copyright 2006 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_SESSION_COMMAND_H_
#define COMPONENTS_SESSIONS_CORE_SESSION_COMMAND_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>

#include "components/sessions/core/sessions_export.h"

namespace base {
class Pickle;
}

namespace sessions {

// SessionCommand contains a command id and arbitrary chunk of data. The id
// and chunk of data are specific to the service creating them.
//
// Both TabRestoreService and SessionService use SessionCommands to represent
// state on disk.
//
// There are two ways to create a SessionCommand:
// . Specify the size of the data block to create. This is useful for
//   commands that have a fixed size.
// . From a pickle, this is useful for commands whose length varies.
class SESSIONS_EXPORT SessionCommand {
 public:
  // These get written to disk, so we define types for them.
  // Type for the identifier.
  using id_type = uint8_t;

  // Type for writing the size.
  using size_type = uint16_t;

  // Creates a session command with the specified id. This allocates a buffer
  // of size |size| that must be filled via contents().
  SessionCommand(id_type id, size_type size);

  // Convenience constructor that creates a session command with the specified
  // id whose contents is populated from the contents of pickle.
  SessionCommand(id_type id, const base::Pickle& pickle);

  SessionCommand(const SessionCommand&) = delete;
  SessionCommand& operator=(const SessionCommand&) = delete;

  // The contents of the command.
  char* contents() { return const_cast<char*>(contents_.c_str()); }
  const char* contents() const { return contents_.c_str(); }
  std::string_view contents_as_string_piece() const {
    return std::string_view(contents_);
  }
  // Identifier for the command.
  id_type id() const { return id_; }

  // Size of data.
  size_type size() const { return static_cast<size_type>(contents_.size()); }

  // The serialized format has overhead (the serialized format includes the
  // id). This returns the size to use when serializing.
  size_type GetSerializedSize() const;

  // Convenience for extracting the data to a target. Returns false if
  // count is not equal to the size of data this command contains.
  bool GetPayload(void* dest, size_t count) const;

  // Returns the contents as a pickle.
  base::Pickle PayloadAsPickle() const;

 private:
  const id_type id_;
  std::string contents_;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_SESSION_COMMAND_H_
