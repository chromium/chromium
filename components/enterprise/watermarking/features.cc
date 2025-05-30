// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/watermarking/features.h"

#include "base/feature_list.h"

namespace enterprise_watermark::features {

BASE_FEATURE(kEnablePrintWatermark,
             "EnablePrintWatermark",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace enterprise_watermark::features
