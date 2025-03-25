// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/local_network_access_permission_context.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"

LocalNetworkAccessPermissionContext::LocalNetworkAccessPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::LOCAL_NETWORK_ACCESS,
          // TODO(crbug.com/400455013): Add permission policy for LNA.
          network::mojom::PermissionsPolicyFeature::kNotFound) {}

LocalNetworkAccessPermissionContext::~LocalNetworkAccessPermissionContext() =
    default;
