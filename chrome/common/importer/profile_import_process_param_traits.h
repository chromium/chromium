// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IMPORTER_PROFILE_IMPORT_PROCESS_PARAM_TRAITS_H_
#define CHROME_COMMON_IMPORTER_PROFILE_IMPORT_PROCESS_PARAM_TRAITS_H_

#include <string>

#include "base/notreached.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/importer/profile_import.mojom.h"
#include "chrome/common/importer/profile_import_process_param_traits_macros.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<chrome::mojom::ImportedPasswordForm::Scheme,
                  importer::ImportedPasswordForm::Scheme> {
  static chrome::mojom::ImportedPasswordForm::Scheme ToMojom(
      importer::ImportedPasswordForm::Scheme input) {
    switch (input) {
      case importer::ImportedPasswordForm::Scheme::kHtml:
        return chrome::mojom::ImportedPasswordForm::Scheme::kHtml;
      case importer::ImportedPasswordForm::Scheme::kBasic:
        return chrome::mojom::ImportedPasswordForm::Scheme::kBasic;
      default:
        break;
    }
    NOTREACHED_IN_MIGRATION();
    return chrome::mojom::ImportedPasswordForm::Scheme::kHtml;
  }

  static bool FromMojom(chrome::mojom::ImportedPasswordForm::Scheme input,
                        importer::ImportedPasswordForm::Scheme* out) {
    switch (input) {
      case chrome::mojom::ImportedPasswordForm::Scheme::kHtml:
        *out = importer::ImportedPasswordForm::Scheme::kHtml;
        return true;
      case chrome::mojom::ImportedPasswordForm::Scheme::kBasic:
        *out = importer::ImportedPasswordForm::Scheme::kBasic;
        return true;
    }
    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

template <>
struct StructTraits<chrome::mojom::ImportedPasswordFormDataView,
                    importer::ImportedPasswordForm> {
  static importer::ImportedPasswordForm::Scheme scheme(
      const importer::ImportedPasswordForm& r) {
    return r.scheme;
  }

  static const std::string& signon_realm(
      const importer::ImportedPasswordForm& r) {
    return r.signon_realm;
  }

  static const GURL& url(const importer::ImportedPasswordForm& r) {
    return r.url;
  }

  static const GURL& action(const importer::ImportedPasswordForm& r) {
    return r.action;
  }

  static const std::u16string& username_element(
      const importer::ImportedPasswordForm& r) {
    return r.username_element;
  }

  static const std::u16string& username_value(
      const importer::ImportedPasswordForm& r) {
    return r.username_value;
  }

  static const std::u16string& password_element(
      const importer::ImportedPasswordForm& r) {
    return r.password_element;
  }

  static const std::u16string& password_value(
      const importer::ImportedPasswordForm& r) {
    return r.password_value;
  }

  static bool blocked_by_user(const importer::ImportedPasswordForm& r) {
    return r.blocked_by_user;
  }

  static bool Read(chrome::mojom::ImportedPasswordFormDataView data,
                   importer::ImportedPasswordForm* out);
};

}  // namespace mojo

#endif  // CHROME_COMMON_IMPORTER_PROFILE_IMPORT_PROCESS_PARAM_TRAITS_H_
