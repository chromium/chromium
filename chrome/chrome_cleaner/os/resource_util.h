// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_RESOURCE_UTIL_H_
#define CHROME_CHROME_CLEANER_OS_RESOURCE_UTIL_H_

#include <stdint.h>

#include "base/strings/string_piece.h"

namespace chrome_cleaner {

// Load the resource |resource_id| from the application resources. On success,
// |output| contains the raw bytes. Return false if the resource can't be found.
bool LoadResourceOfKind(uint32_t resource_id,
                        const wchar_t* kind,
                        base::StringPiece* output);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_RESOURCE_UTIL_H_
