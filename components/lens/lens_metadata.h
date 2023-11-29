// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_METADATA_H_
#define COMPONENTS_LENS_LENS_METADATA_H_

#include "components/lens/lens_metadata.mojom.h"
#include "components/lens/proto/v1/lens_latencies_metadata.pb.h"
#include "ui/gfx/geometry/size_f.h"

namespace LensMetadata {
std::string CreateProto(
    const std::vector<lens::mojom::LatencyLogPtr>& log_data);
}

#endif  // COMPONENTS_LENS_LENS_METADATA_H_
