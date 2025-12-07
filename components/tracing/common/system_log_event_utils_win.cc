// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/system_log_event_utils_win.h"

#include <windows.h>

#include "base/containers/span.h"
#include "base/strings/string_view_util.h"

namespace {

template <class CharT>
std::optional<std::basic_string<CharT>> CopyBasicString(
    base::BufferIterator<const uint8_t>& iterator) {
  auto before = iterator;

  // Advance `iterator` forward until a terminator is found.
  while (true) {
    // Use `CopyObject()` to avoid unaligned reads for multi-byte char strings.
    auto optional_char = iterator.CopyObject<CharT>();
    if (!optional_char.has_value()) {    // Premature end of data.
      iterator.Seek(before.position());  // Rewind.
      return std::nullopt;
    }
    if (!*optional_char) {
      break;  // String terminator found.
    }
  }

  // Compute the string length, in chars, excluding the terminator.
  auto size = (iterator.position() - before.position()) / sizeof(CharT) - 1;
  return std::basic_string<CharT>(
      base::as_string_view(before.Span<CharT>(size)));
}

}  // namespace

namespace tracing {

std::optional<std::string> CopyString(
    base::BufferIterator<const uint8_t>& iterator) {
  return CopyBasicString<char>(iterator);
}

std::optional<std::wstring> CopyWString(
    base::BufferIterator<const uint8_t>& iterator) {
  return CopyBasicString<wchar_t>(iterator);
}

std::optional<base::win::Sid> CopySid(
    size_t pointer_size,
    base::BufferIterator<const uint8_t>& iterator) {
  // Operate on a copy of the caller's iterator so that the caller's is only
  // modified on success.
  base::BufferIterator<const uint8_t> temp_iter(iterator);

  // Source:
  // https://learn.microsoft.com/en-us/windows/win32/etw/event-tracing-mof-qualifiers.

  // "If the first 4-bytes (ULONG) of the blob is nonzero, the blob contains a
  // SID." If the blob contains a SID, the first two `pointer_size` elements are
  // a `TOKEN_USER` struct and the remainder is a `SID` struct. In practice, a
  // SID is present even if the first 32-bit int is zero, so ignore its value,
  // and skip the `TOKEN_USER` struct (two pointers-sized elements) to reach the
  // bytes holding the SID.
  if (temp_iter.Object<uint32_t>() == nullptr ||
      temp_iter.Span<uint8_t>(2 * pointer_size).empty()) {
    return std::nullopt;
  }

  const auto sid_position = temp_iter.position();
  const auto bytes_remaining = temp_iter.total_size() - sid_position;
  if (bytes_remaining < 2) {
    return std::nullopt;
  }
  auto remaining_span = temp_iter.Span<uint8_t>(bytes_remaining);
  // The second byte in `remaining_span` is the SubAuthorityCount byte. Use it
  // to ensure that there is enough data remaining to hold the indicated number
  // of subauthorities as per the citation above.
  if (bytes_remaining < sizeof(DWORD) * remaining_span[1] + 8) {
    return std::nullopt;
  }
  const SID* sid = reinterpret_cast<const SID*>(remaining_span.data());
  if (!::IsValidSid(const_cast<SID*>(sid))) {
    return std::nullopt;
  }
  const auto sid_length = ::GetLengthSid(const_cast<SID*>(sid));
  if (sid_length > remaining_span.size()) {
    return std::nullopt;
  }

  // Advance the caller's iterator to just past the sid.
  iterator.Seek(sid_position + sid_length);
  // Return the SID.
  return base::win::Sid::FromPSID(const_cast<SID*>(sid));
}

}  // namespace tracing
