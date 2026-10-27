// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PASSWORD_MANAGER_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PASSWORD_MANAGER_MOJOM_TRAITS_H_

#include <string>

#include "chrome/browser/password_manager/password_change/password_change_from_checkup_delegate.h"
#include "chrome/browser/ui/webui/password_manager/password_manager.mojom-data-view.h"
#include "components/password_manager/core/browser/export/export_progress_status.h"
#include "components/password_manager/core/browser/password_store/actionable_error.h"
#include "components/password_manager/core/browser/ui/actor_login_permission.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "url/gurl.h"

namespace mojo {

template <>
struct EnumTraits<password_manager::mojom::ExportProgressStatus,
                  password_manager::ExportProgressStatus> {
  static password_manager::mojom::ExportProgressStatus ToMojom(
      password_manager::ExportProgressStatus input);
  static password_manager::ExportProgressStatus FromMojom(
      password_manager::mojom::ExportProgressStatus input);
};

template <>
struct EnumTraits<
    password_manager::mojom::PasswordAutomaticChangeState,
    PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState> {
  static password_manager::mojom::PasswordAutomaticChangeState ToMojom(
      PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState input);
  static PasswordChangeFromCheckupDelegate::PasswordAutomaticChangeState
  FromMojom(password_manager::mojom::PasswordAutomaticChangeState input);
};

template <>
struct EnumTraits<password_manager::mojom::PasswordManagerActionableError,
                  password_manager::ActionableError> {
  static password_manager::mojom::PasswordManagerActionableError ToMojom(
      password_manager::ActionableError input);
  static password_manager::ActionableError FromMojom(
      password_manager::mojom::PasswordManagerActionableError input);
};

template <>
struct StructTraits<password_manager::mojom::DomainInfoDataView,
                    password_manager::CredentialUIEntry::DomainInfo> {
  static const std::string& human_readable_name(
      const password_manager::CredentialUIEntry::DomainInfo& r) {
    return r.name;
  }

  static const GURL& url(
      const password_manager::CredentialUIEntry::DomainInfo& r) {
    return r.url;
  }

  static const std::string& signon_realm(
      const password_manager::CredentialUIEntry::DomainInfo& r) {
    return r.signon_realm;
  }

  static bool Read(password_manager::mojom::DomainInfoDataView data,
                   password_manager::CredentialUIEntry::DomainInfo* out);
};

template <>
struct StructTraits<password_manager::mojom::ActorLoginPermissionDataView,
                    password_manager::ActorLoginPermission> {
  static const password_manager::CredentialUIEntry::DomainInfo& domain_info(
      const password_manager::ActorLoginPermission& r) {
    return r.domain_info;
  }

  static const GURL& favicon_url(
      const password_manager::ActorLoginPermission& r) {
    return r.favicon_url;
  }

  static const std::u16string& username(
      const password_manager::ActorLoginPermission& r) {
    return r.username;
  }

  static bool Read(password_manager::mojom::ActorLoginPermissionDataView data,
                   password_manager::ActorLoginPermission* out);
};

}  // namespace mojo

#endif  // CHROME_BROWSER_UI_WEBUI_PASSWORD_MANAGER_PASSWORD_MANAGER_MOJOM_TRAITS_H_
