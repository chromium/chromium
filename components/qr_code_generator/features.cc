// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/qr_code_generator/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace qr_code_generator {

BASE_FEATURE(kRustyQrCodeGeneratorFeature,
             "RustyQrCodeGenerator",
#if BUILDFLAG(IS_CHROMEOS)
             // TODO(https://crbug.com/1431991): Enable on all platforms.
             // (http://crbug.com/1467360 is the only known blocker on CrOS.)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

}  // namespace qr_code_generator
