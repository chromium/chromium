// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/quarantine/quarantine.h"

#include "build/build_config.h"

#if !defined(OS_WIN) && !defined(OS_MACOSX) && !defined(OS_LINUX)

namespace download {

QuarantineFileResult QuarantineFile(const base::FilePath& file,
                                    const GURL& source_url,
                                    const GURL& referrer_url,
                                    const std::string& client_guid) {
  return QuarantineFileResult::OK;
}

}  // namespace download

#endif  // !WIN && !MAC && !LINUX
