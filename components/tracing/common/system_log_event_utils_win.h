// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains various helper functions for parsing data from buffers
// holding encoded data from the Windows system trace provider.

#ifndef COMPONENTS_TRACING_COMMON_SYSTEM_LOG_EVENT_UTILS_WIN_H_
#define COMPONENTS_TRACING_COMMON_SYSTEM_LOG_EVENT_UTILS_WIN_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>

#include "base/containers/buffer_iterator.h"
#include "base/win/sid.h"
#include "components/tracing/tracing_export.h"

namespace tracing {

// Returns a null-terminated character string from `iterator`, leaving it at the
// byte following the terminator. Returns nullopt and leaves `iterator` at its
// previous position if no data remains or if no string terminator is found.
TRACING_EXPORT std::optional<std::string> CopyString(
    base::BufferIterator<const uint8_t>& iterator);

// Returns a null-terminated wide character string from `iterator`, leaving it
// at the byte following the terminator. Returns nullopt and leaves `iterator`
// at its previous position if no data remains or if no string terminator is
// found.
TRACING_EXPORT std::optional<std::wstring> CopyWString(
    base::BufferIterator<const uint8_t>& iterator);

// Returns a SID from `iterator` encoded with the given pointer size, or nullopt
// (leaving `iterator` at its previous position) if the data is malformed.
TRACING_EXPORT std::optional<base::win::Sid> CopySid(
    size_t pointer_size,
    base::BufferIterator<const uint8_t>& iterator);

}  // namespace tracing

#endif  // COMPONENTS_TRACING_COMMON_SYSTEM_LOG_EVENT_UTILS_WIN_H_
