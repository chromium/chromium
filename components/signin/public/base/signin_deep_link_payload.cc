// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_deep_link_payload.h"

namespace signin {

bool SigninDeepLinkPayload::HasAllRequiredFields() const {
  return entry_point_id.has_value() &&
         entry_point_id_raw_value_for_metrics.has_value() && email.has_value();
}

}  // namespace signin
