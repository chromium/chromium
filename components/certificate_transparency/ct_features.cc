// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/ct_features.h"

#include "build/build_config.h"

namespace certificate_transparency {
namespace features {

const base::Feature kCertificateTransparencyComponentUpdater{
    "CertificateTransparencyComponentUpdater",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCertificateTransparency2022Policy{
    "CertificateTransparency2022Policy", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCertificateTransparency2022PolicyAllCerts{
    "CertificateTransparency2022PolicyAllCerts",
    base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace certificate_transparency
