// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/note_features.h"

namespace content_creation {

BASE_FEATURE(kWebNotesStylizeEnabled,
             "WebNotesStylize",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<bool> kRandomizeOrderParam{&kWebNotesStylizeEnabled,
                                                    "randomize_order", false};

bool IsStylizeEnabled() {
  return base::FeatureList::IsEnabled(kWebNotesStylizeEnabled);
}

bool IsRandomizeOrderEnabled() {
  DCHECK(IsStylizeEnabled());
  return kRandomizeOrderParam.Get();
}

}  // namespace content_creation
