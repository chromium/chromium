// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/test_support.h"

#include "build/build_config.h"

#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_MAC)

namespace quarantine {

bool IsFileQuarantined(const base::FilePath& file,
                       const GURL& source_url,
                       const GURL& referrer_url) {
  return false;
}

}  // namespace quarantine

#endif  // !WIN && !MAC
