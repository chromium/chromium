// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/test_support.h"

#include "build/build_config.h"

#if !defined(OS_WIN) && !defined(OS_MACOSX)

namespace quarantine {

bool IsFileQuarantined(const base::FilePath& file,
                       const GURL& source_url,
                       const GURL& referrer_url) {
  return false;
}

}  // namespace quarantine

#endif  // !WIN && !MAC
