// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CONTROLLED_FRAME_CONTROLLED_FRAME_H_
#define CHROME_COMMON_CONTROLLED_FRAME_CONTROLLED_FRAME_H_

#include <memory>
#include <string>

#include "extensions/buildflags/buildflags.h"
#include "extensions/common/context_data.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/context_type.mojom-forward.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class GURL;

namespace controlled_frame {

// Returns the availability of an API that requires //chrome-level checks (as
// opposed to //extensions-level checks). For certain platforms (e.g. Android)
// some features may always return false.
bool AvailabilityCheck(const std::string& api_full_name,
                       const extensions::Extension* extension,
                       extensions::mojom::ContextType context,
                       const GURL& url,
                       extensions::Feature::Platform platform,
                       int context_id,
                       bool check_developer_mode,
                       const extensions::ContextData& context_data);

// Creates the availability check map for controlled frame and related APIs
// (e.g. webViewInternal).
extensions::Feature::FeatureDelegatedAvailabilityCheckMap
CreateAvailabilityCheckMap();

}  // namespace controlled_frame

#endif  // CHROME_COMMON_CONTROLLED_FRAME_CONTROLLED_FRAME_H_
