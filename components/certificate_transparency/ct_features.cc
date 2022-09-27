// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/ct_features.h"

#include "build/build_config.h"

namespace certificate_transparency {
namespace features {

BASE_FEATURE(kCertificateTransparencyComponentUpdater,
             "CertificateTransparencyComponentUpdater",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features
}  // namespace certificate_transparency
