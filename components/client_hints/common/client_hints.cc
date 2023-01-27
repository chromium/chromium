// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/client_hints/common/client_hints.h"

#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"

namespace client_hints {

void GetAllowedClientHints(const base::Value& client_hints_rule,
                           blink::EnabledClientHints* client_hints) {
  if (client_hints_rule.is_none()) {
    return;
  }

  DCHECK(client_hints_rule.is_dict());
  const base::Value* list_value =
      client_hints_rule.GetDict().Find(kClientHintsSettingKey);
  if (list_value == nullptr) {
    return;
  }

  // We should guarantee client hints list value always be Type::List since we
  // save the client hints as base::Value::List in the Prefs. For details,
  // check components/client_hints/browser/client_hints.cc
  DCHECK(list_value->is_list());
  for (const auto& client_hint : list_value->GetList()) {
    DCHECK(client_hint.is_int());
    network::mojom::WebClientHintsType client_hint_mojo =
        static_cast<network::mojom::WebClientHintsType>(client_hint.GetInt());
    if (network::mojom::IsKnownEnumValue(client_hint_mojo)) {
      client_hints->SetIsEnabled(client_hint_mojo, true);
    }
  }
}

}  // namespace client_hints
