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

BASE_FEATURE(kWebNotesPublish,
             "WebNotesPublish",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebNotesDynamicTemplates,
             "WebNotesDynamicTemplates",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsStylizeEnabled() {
  return base::FeatureList::IsEnabled(kWebNotesStylizeEnabled);
}

bool IsRandomizeOrderEnabled() {
  DCHECK(IsStylizeEnabled());
  return kRandomizeOrderParam.Get();
}

bool IsPublishEnabled() {
  return base::FeatureList::IsEnabled(kWebNotesPublish);
}

bool IsDynamicTemplatesEnabled() {
  return base::FeatureList::IsEnabled(kWebNotesDynamicTemplates);
}

}  // namespace content_creation
