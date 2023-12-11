// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CONTROLLED_FRAME_CONTROLLED_FRAME_H_
#define CHROME_COMMON_CONTROLLED_FRAME_CONTROLLED_FRAME_H_

#include <memory>
#include <string>

#include "extensions/common/context_data.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/context_type.mojom-forward.h"

class GURL;

namespace controlled_frame {

bool AvailabilityCheck(const std::string& api_full_name,
                       const extensions::Extension* extension,
                       extensions::mojom::ContextType context,
                       const GURL& url,
                       extensions::Feature::Platform platform,
                       int context_id,
                       bool check_developer_mode,
                       const extensions::ContextData& context_data);

extensions::Feature::FeatureDelegatedAvailabilityCheckMap
CreateAvailabilityCheckMap();

}  // namespace controlled_frame

#endif  // CHROME_COMMON_CONTROLLED_FRAME_CONTROLLED_FRAME_H_
