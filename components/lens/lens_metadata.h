// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_METADATA_H_
#define COMPONENTS_LENS_LENS_METADATA_H_

#include <string>
#include <vector>

#include "components/lens/lens_metadata.mojom.h"

namespace LensMetadata {

std::string CreateProto(
    const std::vector<lens::mojom::LatencyLogPtr>& log_data);

}  // namespace LensMetadata

#endif  // COMPONENTS_LENS_LENS_METADATA_H_
