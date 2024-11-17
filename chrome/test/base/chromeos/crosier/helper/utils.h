// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROMEOS_CROSIER_HELPER_UTILS_H_
#define CHROME_TEST_BASE_CHROMEOS_CROSIER_HELPER_UTILS_H_

#include <string>
#include <string_view>

#include "base/files/scoped_file.h"

namespace base {
class FilePath;
}

namespace crosier {

// Creates a unix domain socket and binds it with `path`.
base::ScopedFD CreateSocketAndBind(const base::FilePath& path);

// Reads a null terminated string from the given socket.
std::string ReadString(const base::ScopedFD& sock);

// Reads `byte_size` bytes from the socket into the given buffer.
void ReadBuffer(const base::ScopedFD& sock, void* buf, int byte_size);

// Sends a string with a null terminator appended to the given socket.
void SendString(const base::ScopedFD& sock, std::string_view str);

}  // namespace crosier

#endif  // CHROME_TEST_BASE_CHROMEOS_CROSIER_HELPER_UTILS_H_
