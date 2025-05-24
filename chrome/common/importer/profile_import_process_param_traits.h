// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IMPORTER_PROFILE_IMPORT_PROCESS_PARAM_TRAITS_H_
#define CHROME_COMMON_IMPORTER_PROFILE_IMPORT_PROCESS_PARAM_TRAITS_H_

#include <string>

#include "base/notreached.h"
#include "chrome/common/importer/profile_import.mojom.h"
#include "chrome/common/importer/profile_import_process_param_traits_macros.h"
#include "components/user_data_importer/common/importer_data_types.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<chrome::mojom::ImportedPasswordForm::Scheme,
                  user_data_importer::ImportedPasswordForm::Scheme> {
  static chrome::mojom::ImportedPasswordForm::Scheme ToMojom(
      user_data_importer::ImportedPasswordForm::Scheme input) {
    switch (input) {
      case user_data_importer::ImportedPasswordForm::Scheme::kHtml:
        return chrome::mojom::ImportedPasswordForm::Scheme::kHtml;
      case user_data_importer::ImportedPasswordForm::Scheme::kBasic:
        return chrome::mojom::ImportedPasswordForm::Scheme::kBasic;
      default:
        break;
    }
    NOTREACHED();
  }

  static bool FromMojom(chrome::mojom::ImportedPasswordForm::Scheme input,
                        user_data_importer::ImportedPasswordForm::Scheme* out) {
    switch (input) {
      case chrome::mojom::ImportedPasswordForm::Scheme::kHtml:
        *out = user_data_importer::ImportedPasswordForm::Scheme::kHtml;
        return true;
      case chrome::mojom::ImportedPasswordForm::Scheme::kBasic:
        *out = user_data_importer::ImportedPasswordForm::Scheme::kBasic;
        return true;
    }
    NOTREACHED();
  }
};

template <>
struct StructTraits<chrome::mojom::ImportedPasswordFormDataView,
                    user_data_importer::ImportedPasswordForm> {
  static user_data_importer::ImportedPasswordForm::Scheme scheme(
      const user_data_importer::ImportedPasswordForm& r) {
    return r.scheme;
  }

  static const std::string& signon_realm(
      const user_data_importer::ImportedPasswordForm& r) {
    return r.signon_realm;
  }

  static const GURL& url(const user_data_importer::ImportedPasswordForm& r) {
    return r.url;
  }

  static const GURL& action(const user_data_importer::ImportedPasswordForm& r) {
    return r.action;
  }

  static const std::u16string& username_element(
      const user_data_importer::ImportedPasswordForm& r) {
    return r.username_element;
  }

  static const std::u16string& username_value(
      const user_data_importer::ImportedPasswordForm& r) {
    return r.username_value;
  }

  static const std::u16string& password_element(
      const user_data_importer::ImportedPasswordForm& r) {
    return r.password_element;
  }

  static const std::u16string& password_value(
      const user_data_importer::ImportedPasswordForm& r) {
    return r.password_value;
  }

  static bool blocked_by_user(
      const user_data_importer::ImportedPasswordForm& r) {
    return r.blocked_by_user;
  }

  static bool Read(chrome::mojom::ImportedPasswordFormDataView data,
                   user_data_importer::ImportedPasswordForm* out);
};

}  // namespace mojo

#endif  // CHROME_COMMON_IMPORTER_PROFILE_IMPORT_PROCESS_PARAM_TRAITS_H_
