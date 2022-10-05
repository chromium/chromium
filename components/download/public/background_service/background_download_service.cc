// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/background_service/background_download_service.h"

namespace download {
#if BUILDFLAG(IS_IOS)
const char kBackgroundDownloadIdentifierPrefix[] = "background_download";
#endif  // BUILDFLAG(IS_IOS)
}  // namespace download
