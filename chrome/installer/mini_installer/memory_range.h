// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_MINI_INSTALLER_MEMORY_RANGE_H_
#define CHROME_INSTALLER_MINI_INSTALLER_MEMORY_RANGE_H_

#include <stddef.h>
#include <stdint.h>

namespace mini_installer {

// Represents a range of bytes in the process's address space.
struct MemoryRange {
  const uint8_t* data = nullptr;
  size_t size = 0;

  bool empty() const { return size == 0; }
};

}  // namespace mini_installer

#endif  // CHROME_INSTALLER_MINI_INSTALLER_MEMORY_RANGE_H_
