// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/identity_credential/identity_credential.h"

#include "ui/gfx/image/image_skia.h"

namespace autofill {

IdentityCredential::IdentityCredential(
    const GURL& idp_config_url,
    const std::string& account_id,
    const std::u16string& idp_for_display,
    const std::u16string& main_text,
    std::map<FieldType, std::u16string> fields,
    const gfx::Image& custom_icon)
    : idp_config_url(idp_config_url),
      account_id(account_id),
      idp_for_display(idp_for_display),
      main_text(main_text),
      fields(fields),
      custom_icon(custom_icon) {}

IdentityCredential::~IdentityCredential() = default;
IdentityCredential::IdentityCredential(const IdentityCredential&) = default;
IdentityCredential& IdentityCredential::operator=(const IdentityCredential&) =
    default;
IdentityCredential::IdentityCredential(IdentityCredential&&) = default;
IdentityCredential& IdentityCredential::operator=(IdentityCredential&&) =
    default;

bool IdentityCredential::operator==(const IdentityCredential& other) const =
    default;

}  // namespace autofill
