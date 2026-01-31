// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/valuables/valuable_types.h"

#include <cstdint>

#include "base/time/time.h"

namespace autofill {

ValuableMetadata::ValuableMetadata(ValuableId valuable_id,
                                   base::Time use_date,
                                   int64_t use_count)
    : valuable_id(std::move(valuable_id)),
      use_date(use_date),
      use_count(use_count) {}

ValuableMetadata::ValuableMetadata(const ValuableMetadata&) = default;
ValuableMetadata::ValuableMetadata(ValuableMetadata&&) = default;

ValuableMetadata::~ValuableMetadata() = default;

}  // namespace autofill
