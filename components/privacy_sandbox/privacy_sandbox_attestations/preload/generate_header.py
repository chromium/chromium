# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys


def _ReadVersionFromJson():
    with open(sys.argv[1], 'r') as f:
        data = json.load(f)
        return data['version']


def main():
    version = _ReadVersionFromJson()
    with open(os.path.join(sys.argv[2], "android_apk_assets.h"), 'w') as f:
        f.write("""\
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRELOAD_ANDROID_APK_ASSETS_H_
#define COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRELOAD_ANDROID_APK_ASSETS_H_

#include <string_view>

namespace privacy_sandbox {

inline constexpr std::string_view kAttestationsListAssetPath =
    "assets/privacy_sandbox_attestations/privacy-sandbox-attestations.dat";

inline constexpr std::string_view kAttestationsListAssetVersion =
    "%(version)s";

}  // namespace privacy_sandbox

#endif  // COMPONENTS_PRIVACY_SANDBOX_PRIVACY_SANDBOX_ATTESTATIONS_PRELOAD_ANDROID_APK_ASSETS_H_
""" % {'version': version})


if __name__ == '__main__':
    main()
