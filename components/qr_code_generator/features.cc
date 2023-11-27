// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/qr_code_generator/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace qr_code_generator {

BASE_FEATURE(kRustyQrCodeGeneratorFeature,
             "RustyQrCodeGenerator",
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

}  // namespace qr_code_generator
