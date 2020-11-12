// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_provider.h"

namespace content_settings {
std::unique_ptr<RuleIterator> ProviderInterface::GetDiscardedRuleIterator(
    ContentSettingsType content_type,
    bool incognito) const {
  return std::make_unique<EmptyRuleIterator>();
}
}  // namespace content_settings