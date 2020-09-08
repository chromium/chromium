// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/client_hints/common/client_hints.h"

#include "third_party/blink/public/common/loader/network_utils.h"
#include "third_party/blink/public/platform/web_client_hints_type.h"
#include "url/gurl.h"

namespace client_hints {

void GetAllowedClientHintsFromSource(
    const GURL& url,
    const ContentSettingsForOneType& client_hints_rules,
    blink::WebEnabledClientHints* client_hints) {
  if (client_hints_rules.empty())
    return;

  if (!blink::network_utils::IsOriginSecure(url))
    return;

  const GURL& origin = url.GetOrigin();

  for (const auto& rule : client_hints_rules) {
    // Look for an exact match since persisted client hints are disabled by
    // default, and enabled only on per-host basis.
    if (rule.primary_pattern == ContentSettingsPattern::Wildcard() ||
        !rule.primary_pattern.Matches(origin)) {
      continue;
    }

    // Found an exact match.
    DCHECK(ContentSettingsPattern::Wildcard() == rule.secondary_pattern);
    DCHECK(rule.setting_value.is_dict());
    const base::Value* expiration_time =
        rule.setting_value.FindKey("expiration_time");

    // |expiration_time| may be null in rare cases. See
    // https://bugs.chromium.org/p/chromium/issues/detail?id=942398.
    if (expiration_time == nullptr)
      continue;
    DCHECK(expiration_time->is_double());

    if (base::Time::Now().ToDoubleT() > expiration_time->GetDouble()) {
      // The client hint is expired.
      return;
    }

    const base::Value* list_value = rule.setting_value.FindKey("client_hints");
    if (list_value == nullptr)
      continue;
    DCHECK(list_value->is_list());
    for (const auto& client_hint : list_value->GetList()) {
      DCHECK(client_hint.is_int());
      network::mojom::WebClientHintsType client_hint_mojo =
          static_cast<network::mojom::WebClientHintsType>(client_hint.GetInt());
      if (network::mojom::IsKnownEnumValue(client_hint_mojo))
        client_hints->SetIsEnabled(client_hint_mojo, true);
    }
    // Match found for |url| and client hints have been set.
    return;
  }
}

}  // namespace client_hints
