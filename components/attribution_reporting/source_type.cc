// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/attribution_reporting/source_type.h"

#include "components/attribution_reporting/source_type.mojom.h"

namespace attribution_reporting {

const char* SourceTypeName(mojom::SourceType source_type) {
  switch (source_type) {
    case mojom::SourceType::kNavigation:
      return "navigation";
    case mojom::SourceType::kEvent:
      return "event";
  }
}

}  // namespace attribution_reporting
