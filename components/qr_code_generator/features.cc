// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/qr_code_generator/features.h"

#include "base/feature_list.h"

BASE_FEATURE(kRustyQrCodeGeneratorFeature,
             "RustyQrCodeGenerator",
             base::FEATURE_DISABLED_BY_DEFAULT);
