// Copyright 2006 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_SESSION_COMMAND_H_
#define COMPONENTS_SESSIONS_CORE_SESSION_COMMAND_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/sessions/core/sessions_export.h"

namespace base {
class Pickle;
class PickleIterator;
}  // namespace base

namespace sessions {

// SessionCommand stores information used for restoring a session.
// It contains an identifier ("id") and arbitrary chunk of data ("contents").
//
// The meaning of the identifier and the contents are specific to the service
// creating them.
//
// Both TabRestoreService and SessionService use SessionCommands to store data
// on disk using the CommandStorageManager.
//
// There are two ways to create a SessionCommand:
// . Specify the size of the data block to create. This is useful for
//   commands that have a fixed size.
// . From a pickle, this is useful for commands whose length varies.
class SESSIONS_EXPORT SessionCommand {
 public:
  // The type of the identifier.
  using id_type = uint8_t;

  // The type of the size of the contents.
  using size_type = uint16_t;

  // Creates a session command with the specified id. This allocates a buffer
  // of size |size| that must be filled via contents().
  SessionCommand(id_type id, size_type size);

  // Convenience constructor that creates a session command with the specified
  // id whose contents is populated from the contents of pickle.
  SessionCommand(id_type id, const base::Pickle& pickle);

  SessionCommand(const SessionCommand&) = delete;
  SessionCommand& operator=(const SessionCommand&) = delete;

  // An identifier for the command.  The meaning of the identifier is specific
  // to the service that creates the command.
  id_type id() const { return id_; }

  // The maximum size of the |contents|.
  // Note that this is less than UINT16_MAX.
  // If this size is exceeded, the contents will be truncated in Serialize().
  static constexpr size_t kMaxContentSize =
      std::numeric_limits<size_type>::max() - sizeof(id_type);

  // The contents of the command.  This is an arbitrary chunk of data whose
  // meaning is specific to the service that creates the command.
  base::span<const uint8_t> contents() const {
    return base::as_byte_span(contents_);
  }
  base::span<uint8_t> contents() {
    return base::as_writable_byte_span(contents_);
  }

  // Convenience for extracting the data to a target. Returns false if
  // count is not equal to the size of data this command contains.
  bool GetContents(void* dest, size_t count) const;

  // Returns an iterator for reading the contents.
  base::PickleIterator ContentsAsPickle() const;

  // Serializes the SessionCommand (e.g., so that it can be written to a file).
  // The serialized form includes the size of the command, the id, and the
  // contents.
  // If the size of the contents is greater than kMaxContentSize, the contents
  // will be truncated.
  // If encryptor is nullptr, the contents will be cleartext.
  // If an encryptor is provided, the contents will be encrypted.
  // If an error occurs, an empty vector will be returned.
  std::vector<uint8_t> Serialize(os_crypt_async::Encryptor* encryptor) const;

  // Deserializes the data into a SessionCommand (e.g., for reading from a
  // file). It is expected that the data was serialized using Serialize().
  // Returns the total serialized size of the command if there is enough data
  // to determine it, or std::nullopt otherwise.
  // encrypted: whether the data was encrypted during serialization.
  static std::optional<size_t> GetSerializedSize(base::span<const uint8_t> data,
                                                 bool encrypted);

  // Deserializes a SessionCommand that was serialized using Serialize()
  // E.g., for reading from a file.
  // If encryptor is nullptr, the data is assumed to be cleartext.
  // If an encryptor is provided, the contents will be decrypted.
  // If an error occurs, nullptr will be returned.
  static std::unique_ptr<SessionCommand> Deserialize(
      base::span<const uint8_t> data,
      os_crypt_async::Encryptor* encryptor);

 private:
  std::vector<uint8_t> SerializeAsCleartext() const;
  std::vector<uint8_t> SerializeWithEncryption(
      const os_crypt_async::Encryptor& encryptor) const;

  static std::unique_ptr<SessionCommand> DeserializeCleartext(
      base::span<const uint8_t> data);
  static std::unique_ptr<SessionCommand> DeserializeEncrypted(
      base::span<const uint8_t> data,
      const os_crypt_async::Encryptor& encryptor);

  const id_type id_;
  std::string contents_;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_SESSION_COMMAND_H_
