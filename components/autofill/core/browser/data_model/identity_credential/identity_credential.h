// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_IDENTITY_CREDENTIAL_IDENTITY_CREDENTIAL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_IDENTITY_CREDENTIAL_IDENTITY_CREDENTIAL_H_

#include "components/autofill/core/browser/field_types.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace autofill {

// Represents a federated identity credential that can be used for autofill.
// This is a simplified version of `content::IdentityRequestAccount` that lives
// in the `//content/public/browser/webid/` layer.
struct IdentityCredential {
  IdentityCredential(const GURL& idp_config_url,
                     const std::string& account_id,
                     const std::u16string& idp_for_display,
                     const std::u16string& main_text,
                     std::map<FieldType, std::u16string> fields,
                     const gfx::Image& custom_icon);

  ~IdentityCredential();
  IdentityCredential(const IdentityCredential&);
  IdentityCredential& operator=(const IdentityCredential&);
  IdentityCredential(IdentityCredential&&);
  IdentityCredential& operator=(IdentityCredential&&);

  bool operator==(const IdentityCredential& other) const;

  GURL idp_config_url;
  std::string account_id;
  std::u16string idp_for_display;
  std::u16string main_text;
  std::map<FieldType, std::u16string> fields;
  gfx::Image custom_icon;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_IDENTITY_CREDENTIAL_IDENTITY_CREDENTIAL_H_
