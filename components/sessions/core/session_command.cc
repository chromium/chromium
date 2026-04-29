// Copyright 2006 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/session_command.h"

#include <algorithm>
#include <limits>
#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/pickle.h"

namespace sessions {

// The encrypted size field is a uint32_t because the encrypted contents can be
// larger than kMaxContentSize.
using encrypted_size_type = uint32_t;

SessionCommand::SessionCommand(id_type id, size_type size)
    : id_(id), contents_(size, 0) {
  // Note that it is possible for size to be greater than kMaxContentSize.
  // This is allowed for historical reasons, the contents will be truncated when
  // Serialize() is called.
}

SessionCommand::SessionCommand(id_type id, const base::Pickle& pickle)
    : id_(id), contents_(pickle.size(), 0) {
  DCHECK(pickle.size() < kMaxContentSize);
  contents().copy_from(pickle);
}

bool SessionCommand::GetContents(void* dest, size_t count) const {
  if (contents_.size() != count) {
    return false;
  }
  UNSAFE_TODO(memcpy(dest, &(contents_[0]), count));
  return true;
}

base::PickleIterator SessionCommand::ContentsAsPickle() const {
  return base::PickleIterator::WithData(contents());
}
std::vector<uint8_t> SessionCommand::Serialize(
    os_crypt_async::Encryptor* encryptor) const {
  if (encryptor) {
    return SerializeWithEncryption(*encryptor);
  } else {
    return SerializeAsCleartext();
  }
}

std::vector<uint8_t> SessionCommand::SerializeAsCleartext() const {
  if (contents().size() > kMaxContentSize) {
    VLOG(2) << "SessionCommand::Serialize: contents_size " << contents().size()
            << " is greater than kMaxContentSize " << kMaxContentSize
            << " and will be truncated.";
  }
  const size_type contents_size = std::min(contents().size(), kMaxContentSize);
  // Note that total_size can be greater that UINT16_MAX, so we use size_t
  // instead of size_type.
  const size_type size_field_value = sizeof(id_type) + contents_size;
  const size_t total_size = sizeof(size_type) + size_field_value;
  std::vector<uint8_t> result(total_size);
  base::span<uint8_t> remaining = base::span(result);
  remaining.take_first(sizeof(size_type))
      .copy_from(base::U16ToNativeEndian(size_field_value));
  remaining.take_first(sizeof(id_type))
      .copy_from(base::byte_span_from_ref(id()));
  // This is where truncation of contents_ can occur.
  remaining.copy_from(contents().first(contents_size));
  return result;
}

std::vector<uint8_t> SessionCommand::SerializeWithEncryption(
    const os_crypt_async::Encryptor& encryptor) const {
  // Emulate the behavior of SerializeAsCleartext() by truncating the contents
  // to kMaxContentSize.
  const size_type contents_size = std::min(contents().size(), kMaxContentSize);
  std::string payload;
  payload.reserve(sizeof(id_type) + contents_size);
  payload.push_back(static_cast<char>(id()));
  const base::span<const uint8_t> contents_span =
      contents().first(contents_size);
  payload.append(reinterpret_cast<const char*>(contents_span.data()),
                 contents_span.size());

  std::optional<std::vector<uint8_t>> encrypted =
      encryptor.EncryptString(payload);
  if (!encrypted ||
      encrypted->size() > std::numeric_limits<encrypted_size_type>::max()) {
    return std::vector<uint8_t>();
  }

  const size_t total_size = sizeof(encrypted_size_type) + encrypted->size();
  std::vector<uint8_t> result(total_size);
  base::span<uint8_t> remaining = base::span(result);
  remaining.take_first<sizeof(encrypted_size_type)>().copy_from(
      base::U32ToNativeEndian(
          static_cast<encrypted_size_type>(encrypted->size())));
  remaining.copy_from(*encrypted);

  return result;
}

std::optional<size_t> SessionCommand::GetSerializedSize(
    base::span<const uint8_t> data,
    bool encrypted) {
  size_t sizeof_size_field =
      encrypted ? sizeof(encrypted_size_type) : sizeof(size_type);
  if (data.size() < sizeof_size_field) {
    // If there's just one byte of data, then it's ignored and not an error.
    return std::nullopt;
  }
  size_t size_field_value =
      encrypted
          ? base::U32FromNativeEndian(data.first<sizeof(encrypted_size_type)>())
          : base::U16FromNativeEndian(data.first<sizeof(size_type)>());
  return sizeof_size_field + size_field_value;
}

std::unique_ptr<SessionCommand> SessionCommand::Deserialize(
    base::span<const uint8_t> data,
    os_crypt_async::Encryptor* encryptor) {
  if (encryptor) {
    return DeserializeEncrypted(data, *encryptor);
  } else {
    return DeserializeCleartext(data);
  }
}

std::unique_ptr<SessionCommand> SessionCommand::DeserializeCleartext(
    base::span<const uint8_t> data) {
  if (data.size() < sizeof(size_type)) {
    VLOG(2) << "SessionCommand::Deserialize: data.size() " << data.size()
            << " is less than sizeof(size_type) " << sizeof(size_type);
    return nullptr;
  }
  base::span<const uint8_t> remaining = data;

  // Parse the size field.
  const size_type size_field_value =
      base::U16FromNativeEndian(remaining.take_first<sizeof(size_type)>());
  if (remaining.size() < size_field_value) {
    VLOG(2) << "SessionCommand::Deserialize: remaining.size() "
            << remaining.size() << " is less than size_field_value "
            << size_field_value;
    return nullptr;
  }
  if (size_field_value < sizeof(id_type)) {
    VLOG(2) << "SessionCommand::Deserialize: size_field_value "
            << size_field_value << " is less than sizeof(id_type) "
            << sizeof(id_type);
    return nullptr;
  }

  // Parse the id field.
  const id_type command_id = remaining.take_first<sizeof(id_type)>()[0];

  // Parse the contents field.
  const size_type content_size = size_field_value - sizeof(id_type);
  std::unique_ptr<sessions::SessionCommand> command =
      std::make_unique<sessions::SessionCommand>(
          command_id, static_cast<size_type>(content_size));
  if (content_size > 0) {
    command->contents().copy_from(remaining.take_first(content_size));
  }
  return command;
}

std::unique_ptr<SessionCommand> SessionCommand::DeserializeEncrypted(
    base::span<const uint8_t> data,
    const os_crypt_async::Encryptor& encryptor) {
  if (data.size() < sizeof(encrypted_size_type)) {
    VLOG(2) << "SessionCommand::DeserializeEncrypted: data.size() "
            << data.size() << " is less than sizeof(encrypted_size_type) "
            << sizeof(encrypted_size_type);
    return nullptr;
  }
  base::span<const uint8_t> remaining = data;

  const encrypted_size_type size_field_value = base::U32FromNativeEndian(
      remaining.take_first<sizeof(encrypted_size_type)>());

  if (remaining.size() < size_field_value) {
    VLOG(2) << "SessionCommand::DeserializeEncrypted: remaining.size() "
            << remaining.size() << " is less than size_field_value "
            << size_field_value;
    return nullptr;
  }

  std::optional<std::string> decrypted =
      encryptor.DecryptData(remaining.take_first(size_field_value));

  if (!decrypted) {
    VLOG(2) << "SessionCommand::DeserializeEncrypted: Decryption failed.";
    return nullptr;
  }

  if (decrypted->size() < sizeof(id_type)) {
    VLOG(2) << "SessionCommand::DeserializeEncrypted: decrypted size "
            << decrypted->size() << " is less than sizeof(id_type) "
            << sizeof(id_type);
    return nullptr;
  }

  const id_type command_id = static_cast<id_type>(decrypted->front());
  const size_t content_size = decrypted->size() - sizeof(id_type);

  if (content_size > std::numeric_limits<size_type>::max()) {
    VLOG(2) << "SessionCommand::DeserializeEncrypted: content_size "
            << content_size << " is greater than max size_type";
    return nullptr;
  }

  std::unique_ptr<sessions::SessionCommand> command =
      std::make_unique<sessions::SessionCommand>(
          command_id, static_cast<size_type>(content_size));
  if (content_size > 0) {
    command->contents().copy_from(
        base::as_byte_span(*decrypted).subspan(sizeof(id_type)));
  }
  return command;
}

}  // namespace sessions
