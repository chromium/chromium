// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_provider.h"
#include "base/notreached.h"

namespace content_settings {

std::unique_ptr<OwnedRule> ProviderInterface::GetRule(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    bool off_the_record,
    const PartitionKey& partition_key) const {
  // TODO(b/316530672): Remove default implementation when all providers are
  // implemented.
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace content_settings
