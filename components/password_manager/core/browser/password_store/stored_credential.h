// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_STORED_CREDENTIAL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_STORED_CREDENTIAL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "components/autofill/core/common/form_data.h"
#include "components/password_manager/core/browser/password_form.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace password_manager {

// StoredCredential represents the data stored in the password store for a
// particular credential. It is a subset of PasswordForm, containing only the
// fields that are persisted or used by the store.
struct StoredCredential {
  StoredCredential();
  StoredCredential(StoredCredential&&);
  StoredCredential& operator=(StoredCredential&&);
  ~StoredCredential();

  // Identification & URLs
  std::optional<FormPrimaryKey> primary_key;
  PasswordForm::Scheme scheme = PasswordForm::Scheme::kHtml;
  std::string signon_realm;
  GURL url;
  GURL action;
  url::SchemeHostPort federation_origin;

  // Elements
  std::u16string submit_element;
  std::u16string username_element;
  std::u16string password_element;

  // Values
  std::u16string username_value;
  std::u16string password_value;
  AlternativeElementVector all_alternative_usernames;

  // Timestamps
  base::Time date_created;
  base::Time date_last_used;
  base::Time date_last_filled;
  base::Time date_password_modified;
  base::Time date_received;

  // Metadata
  bool blocked_by_user = false;
  PasswordForm::Type type = PasswordForm::Type::kFormSubmission;
  int times_used_in_html_form = 0;
  std::u16string display_name;
  GURL icon_url;
  bool skip_zero_click = false;
  PasswordForm::GenerationUploadStatus generation_upload_status =
      PasswordForm::GenerationUploadStatus::kNoSignalSent;

  // Storage Specifics
  PasswordForm::Store in_store = PasswordForm::Store::kNotSet;
  std::vector<signin::GaiaIdHash> moving_blocked_for_list;
  base::flat_map<InsecureType, InsecurityMetadata> password_issues;
  std::vector<PasswordNote> notes;

  // Form Data
  autofill::FormData form_data;

  // iOS Specific
  std::string keychain_identifier;

  // Shared Password Metadata
  std::u16string sender_email;
  std::u16string sender_name;
  bool sharing_notification_displayed = false;
  GURL sender_profile_image_url;

  // Actor Login
  bool actor_login_approved = false;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_STORED_CREDENTIAL_H_
