// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/password_form_converters.h"

#include <utility>

namespace password_manager {

PasswordForm ToPasswordForm(const StoredCredential& cred) {
  PasswordForm form;
  form.primary_key = cred.primary_key;
  form.scheme = cred.scheme;
  form.signon_realm = cred.signon_realm;
  form.affiliated_web_realm = cred.affiliated_web_realm;
  form.url = cred.url;
  form.action = cred.action;
  form.federation_origin = cred.federation_origin;

  form.submit_element = cred.submit_element;
  form.username_element = cred.username_element;
  form.password_element = cred.password_element;
  form.username_value = cred.username_value;
  form.password_value = cred.password_value;
  form.all_alternative_usernames = cred.all_alternative_usernames;
  form.date_created = cred.date_created;
  form.date_last_used = cred.date_last_used;
  form.date_last_filled = cred.date_last_filled;
  form.date_password_modified = cred.date_password_modified;
  form.date_received = cred.date_received;
  form.blocked_by_user = cred.blocked_by_user;
  form.type = cred.type;
  form.times_used_in_html_form = cred.times_used_in_html_form;
  form.display_name = cred.display_name;
  form.icon_url = cred.icon_url;
  form.skip_zero_click = cred.skip_zero_click;
  form.generation_upload_status = cred.generation_upload_status;
  form.in_store = cred.in_store;
  form.moving_blocked_for_list = cred.moving_blocked_for_list;
  form.password_issues = cred.password_issues;
  form.notes = cred.notes;
  form.form_data = cred.form_data;
  form.keychain_identifier = cred.keychain_identifier;
  form.sender_email = cred.sender_email;
  form.sender_name = cred.sender_name;
  form.sharing_notification_displayed = cred.sharing_notification_displayed;
  form.sender_profile_image_url = cred.sender_profile_image_url;
  form.actor_login_approved = cred.actor_login_approved;
  form.app_display_name = cred.app_display_name;
  form.app_icon_url = cred.app_icon_url;
  form.previously_associated_sync_account_email =
      cred.previously_associated_sync_account_email;
  form.match_type = cred.match_type;

  return form;
}

PasswordForm ToPasswordForm(StoredCredential&& cred) {
  PasswordForm form;
  form.primary_key = cred.primary_key;
  form.scheme = cred.scheme;
  form.signon_realm = std::move(cred.signon_realm);
  form.affiliated_web_realm = std::move(cred.affiliated_web_realm);
  form.url = std::move(cred.url);
  form.action = std::move(cred.action);
  form.federation_origin = std::move(cred.federation_origin);

  form.submit_element = std::move(cred.submit_element);
  form.username_element = std::move(cred.username_element);
  form.password_element = std::move(cred.password_element);
  form.username_value = std::move(cred.username_value);
  form.password_value = std::move(cred.password_value);
  form.all_alternative_usernames = std::move(cred.all_alternative_usernames);
  form.date_created = cred.date_created;
  form.date_last_used = cred.date_last_used;
  form.date_last_filled = cred.date_last_filled;
  form.date_password_modified = cred.date_password_modified;
  form.date_received = cred.date_received;
  form.blocked_by_user = cred.blocked_by_user;
  form.type = cred.type;
  form.times_used_in_html_form = cred.times_used_in_html_form;
  form.display_name = std::move(cred.display_name);
  form.icon_url = std::move(cred.icon_url);
  form.skip_zero_click = cred.skip_zero_click;
  form.generation_upload_status = cred.generation_upload_status;
  form.in_store = cred.in_store;
  form.moving_blocked_for_list = std::move(cred.moving_blocked_for_list);
  form.password_issues = std::move(cred.password_issues);
  form.notes = std::move(cred.notes);
  form.form_data = std::move(cred.form_data);
  form.keychain_identifier = std::move(cred.keychain_identifier);
  form.sender_email = std::move(cred.sender_email);
  form.sender_name = std::move(cred.sender_name);
  form.sharing_notification_displayed = cred.sharing_notification_displayed;
  form.sender_profile_image_url = std::move(cred.sender_profile_image_url);
  form.actor_login_approved = cred.actor_login_approved;
  form.app_display_name = std::move(cred.app_display_name);
  form.app_icon_url = std::move(cred.app_icon_url);
  form.previously_associated_sync_account_email =
      std::move(cred.previously_associated_sync_account_email);
  form.match_type = cred.match_type;

  return form;
}

StoredCredential FromPasswordForm(PasswordForm form) {
  StoredCredential cred;
  cred.primary_key = std::move(form.primary_key);
  cred.scheme = form.scheme;
  cred.signon_realm = std::move(form.signon_realm);
  cred.affiliated_web_realm = std::move(form.affiliated_web_realm);
  cred.url = std::move(form.url);
  cred.action = std::move(form.action);
  cred.federation_origin = std::move(form.federation_origin);

  cred.submit_element = std::move(form.submit_element);
  cred.username_element = std::move(form.username_element);
  cred.password_element = std::move(form.password_element);
  cred.username_value = std::move(form.username_value);
  cred.password_value = std::move(form.password_value);
  cred.all_alternative_usernames = std::move(form.all_alternative_usernames);
  cred.date_created = form.date_created;
  cred.date_last_used = form.date_last_used;
  cred.date_last_filled = form.date_last_filled;
  cred.date_password_modified = form.date_password_modified;
  cred.date_received = form.date_received;
  cred.blocked_by_user = form.blocked_by_user;
  cred.type = form.type;
  cred.times_used_in_html_form = form.times_used_in_html_form;
  cred.display_name = std::move(form.display_name);
  cred.icon_url = std::move(form.icon_url);
  cred.skip_zero_click = form.skip_zero_click;
  cred.generation_upload_status = form.generation_upload_status;
  cred.in_store = form.in_store;
  cred.moving_blocked_for_list = std::move(form.moving_blocked_for_list);
  cred.password_issues = std::move(form.password_issues);
  cred.notes = std::move(form.notes);
  cred.form_data = std::move(form.form_data);
  cred.keychain_identifier = std::move(form.keychain_identifier);
  cred.sender_email = std::move(form.sender_email);
  cred.sender_name = std::move(form.sender_name);
  cred.sharing_notification_displayed = form.sharing_notification_displayed;
  cred.sender_profile_image_url = std::move(form.sender_profile_image_url);
  cred.actor_login_approved = form.actor_login_approved;
  cred.app_display_name = std::move(form.app_display_name);
  cred.app_icon_url = std::move(form.app_icon_url);
  cred.previously_associated_sync_account_email =
      std::move(form.previously_associated_sync_account_email);
  cred.match_type = form.match_type;

  return cred;
}

StoredCredential CloneStoredCredential(const StoredCredential& cred) {
  return FromPasswordForm(ToPasswordForm(cred));
}

std::vector<PasswordForm> ToPasswordForms(
    std::vector<StoredCredential>&& credentials) {
  std::vector<PasswordForm> forms;
  forms.reserve(credentials.size());
  for (auto& cred : credentials) {
    forms.push_back(ToPasswordForm(std::move(cred)));
  }
  return forms;
}

std::vector<PasswordForm> ToPasswordForms(
    const std::vector<StoredCredential>& credentials) {
  std::vector<PasswordForm> forms;
  forms.reserve(credentials.size());
  for (const auto& cred : credentials) {
    forms.push_back(ToPasswordForm(cred));
  }
  return forms;
}

std::vector<StoredCredential> FromPasswordForms(
    std::vector<PasswordForm> forms) {
  std::vector<StoredCredential> credentials;
  credentials.reserve(forms.size());
  for (auto& form : forms) {
    credentials.push_back(FromPasswordForm(std::move(form)));
  }
  return credentials;
}

LoginsResultOrError ToLoginsResultOrError(BackendLoginsResultOrError result) {
  if (std::holds_alternative<PasswordStoreBackendError>(result)) {
    return std::get<PasswordStoreBackendError>(result);
  }
  return ToPasswordForms(std::get<BackendLoginsResult>(std::move(result)));
}

}  // namespace password_manager
