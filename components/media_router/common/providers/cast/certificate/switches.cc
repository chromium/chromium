// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/certificate/switches.h"

namespace cast_certificate {
namespace switches {

// When enabled by build flags, passing this argument allows the Cast
// authentication utils to use a custom root developer certificate in the trust
// store instead of the root Google-signed cert.
const char kCastDeveloperCertificatePath[] = "cast-developer-certificate-path";

}  // namespace switches
}  // namespace cast_certificate
