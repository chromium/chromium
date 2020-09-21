// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IMPORTER_PROFILE_IMPORT_PROCESS_PARAM_TRAITS_H_
#define CHROME_COMMON_IMPORTER_PROFILE_IMPORT_PROCESS_PARAM_TRAITS_H_

#include "base/strings/string16.h"
#include "chrome/common/importer/profile_import.mojom.h"
#include "chrome/common/importer/profile_import_process_param_traits_macros.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<chrome::mojom::ImportedPasswordForm::Scheme,
                  autofill::PasswordForm::Scheme> {
  static chrome::mojom::ImportedPasswordForm::Scheme ToMojom(
      autofill::PasswordForm::Scheme input) {
    switch (input) {
      case autofill::PasswordForm::Scheme::kHtml:
        return chrome::mojom::ImportedPasswordForm::Scheme::kHtml;
      case autofill::PasswordForm::Scheme::kBasic:
        return chrome::mojom::ImportedPasswordForm::Scheme::kBasic;
      default:
        break;
    }
    NOTREACHED();
    return chrome::mojom::ImportedPasswordForm::Scheme::kHtml;
  }

  static bool FromMojom(chrome::mojom::ImportedPasswordForm::Scheme input,
                        autofill::PasswordForm::Scheme* out) {
    switch (input) {
      case chrome::mojom::ImportedPasswordForm::Scheme::kHtml:
        *out = autofill::PasswordForm::Scheme::kHtml;
        return true;
      case chrome::mojom::ImportedPasswordForm::Scheme::kBasic:
        *out = autofill::PasswordForm::Scheme::kBasic;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct StructTraits<chrome::mojom::ImportedPasswordFormDataView,
                    autofill::PasswordForm> {
  static autofill::PasswordForm::Scheme scheme(
      const autofill::PasswordForm& r) {
    return r.scheme;
  }

  static const std::string& signon_realm(const autofill::PasswordForm& r) {
    return r.signon_realm;
  }

  static const GURL& url(const autofill::PasswordForm& r) { return r.url; }

  static const GURL& action(const autofill::PasswordForm& r) {
    return r.action;
  }

  static const base::string16& username_element(
      const autofill::PasswordForm& r) {
    return r.username_element;
  }

  static const base::string16& username_value(const autofill::PasswordForm& r) {
    return r.username_value;
  }

  static const base::string16& password_element(
      const autofill::PasswordForm& r) {
    return r.password_element;
  }

  static const base::string16& password_value(const autofill::PasswordForm& r) {
    return r.password_value;
  }

  static bool blocked_by_user(const autofill::PasswordForm& r) {
    return r.blocked_by_user;
  }

  static bool Read(chrome::mojom::ImportedPasswordFormDataView data,
                   autofill::PasswordForm* out);
};

}  // namespace mojo

#endif  // CHROME_COMMON_IMPORTER_PROFILE_IMPORT_PROCESS_PARAM_TRAITS_H_
