// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/password_form_converters.h"

#include "base/strings/utf_string_conversions.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace password_manager {

namespace {

PasswordForm CreateFullPasswordForm() {
  PasswordForm form;
  form.primary_key = FormPrimaryKey(1);
  form.scheme = PasswordForm::Scheme::kHtml;
  form.signon_realm = "https://example.com";
  form.url = GURL("https://example.com/login");
  form.action = GURL("https://example.com/submit");
  form.federation_origin = url::SchemeHostPort(GURL("https://idp.com"));
  form.submit_element = u"submit";
  form.username_element = u"username";
  form.password_element = u"password";
  form.username_value = u"user";
  form.password_value = u"pass";
  form.all_alternative_usernames = {
      AlternativeElement(AlternativeElement::Value(u"alt_user"))};
  form.date_created = base::Time::FromTimeT(100);
  form.date_last_used = base::Time::FromTimeT(200);
  form.date_last_filled = base::Time::FromTimeT(300);
  form.date_password_modified = base::Time::FromTimeT(400);
  form.date_received = base::Time::FromTimeT(500);
  form.blocked_by_user = true;
  form.type = PasswordForm::Type::kFormSubmission;
  form.times_used_in_html_form = 5;
  form.display_name = u"Display Name";
  form.icon_url = GURL("https://example.com/icon.png");
  form.skip_zero_click = true;
  form.generation_upload_status =
      PasswordForm::GenerationUploadStatus::kPositiveSignalSent;
  form.in_store = PasswordForm::Store::kAccountStore;
  form.moving_blocked_for_list = {
      signin::GaiaIdHash::FromGaiaId(GaiaId("gaia_id"))};
  form.password_issues[InsecureType::kLeaked] = InsecurityMetadata();
  form.notes = {PasswordNote(u"note", base::Time::FromTimeT(600))};

  autofill::FormData form_data;
  form_data.set_name(u"form_name");
  form_data.set_url(GURL("https://example.com/form"));
  form.form_data = std::move(form_data);

  form.keychain_identifier = "keychain_id";
  form.sender_email = u"sender@example.com";
  form.sender_name = u"Sender Name";
  form.sharing_notification_displayed = true;
  form.sender_profile_image_url = GURL("https://example.com/sender.png");
  form.actor_login_approved = true;
  return form;
}

StoredCredential CreateFullStoredCredential() {
  StoredCredential cred;
  cred.primary_key = FormPrimaryKey(1);
  cred.scheme = PasswordForm::Scheme::kHtml;
  cred.signon_realm = "https://example.com";
  cred.url = GURL("https://example.com/login");
  cred.action = GURL("https://example.com/submit");
  cred.federation_origin = url::SchemeHostPort(GURL("https://idp.com"));
  cred.submit_element = u"submit";
  cred.username_element = u"username";
  cred.password_element = u"password";
  cred.username_value = u"user";
  cred.password_value = u"pass";
  cred.all_alternative_usernames = {
      AlternativeElement(AlternativeElement::Value(u"alt_user"))};
  cred.date_created = base::Time::FromTimeT(100);
  cred.date_last_used = base::Time::FromTimeT(200);
  cred.date_last_filled = base::Time::FromTimeT(300);
  cred.date_password_modified = base::Time::FromTimeT(400);
  cred.date_received = base::Time::FromTimeT(500);
  cred.blocked_by_user = true;
  cred.type = PasswordForm::Type::kFormSubmission;
  cred.times_used_in_html_form = 5;
  cred.display_name = u"Display Name";
  cred.icon_url = GURL("https://example.com/icon.png");
  cred.skip_zero_click = true;
  cred.generation_upload_status =
      PasswordForm::GenerationUploadStatus::kPositiveSignalSent;
  cred.in_store = PasswordForm::Store::kAccountStore;
  cred.moving_blocked_for_list = {
      signin::GaiaIdHash::FromGaiaId(GaiaId("gaia_id"))};
  cred.password_issues[InsecureType::kLeaked] = InsecurityMetadata();
  cred.notes = {PasswordNote(u"note", base::Time::FromTimeT(600))};

  autofill::FormData form_data;
  form_data.set_name(u"form_name");
  form_data.set_url(GURL("https://example.com/form"));
  cred.form_data = std::move(form_data);

  cred.keychain_identifier = "keychain_id";
  cred.sender_email = u"sender@example.com";
  cred.sender_name = u"Sender Name";
  cred.sharing_notification_displayed = true;
  cred.sender_profile_image_url = GURL("https://example.com/sender.png");
  cred.actor_login_approved = true;
  return cred;
}

void ExpectEqual(const PasswordForm& form, const StoredCredential& cred) {
  EXPECT_EQ(form.primary_key, cred.primary_key);
  EXPECT_EQ(form.scheme, cred.scheme);
  EXPECT_EQ(form.signon_realm, cred.signon_realm);
  EXPECT_EQ(form.url, cred.url);
  EXPECT_EQ(form.action, cred.action);
  EXPECT_EQ(form.federation_origin, cred.federation_origin);
  EXPECT_EQ(form.submit_element, cred.submit_element);
  EXPECT_EQ(form.username_element, cred.username_element);
  EXPECT_EQ(form.password_element, cred.password_element);
  EXPECT_EQ(form.username_value, cred.username_value);
  EXPECT_EQ(form.password_value, cred.password_value);
  EXPECT_EQ(form.all_alternative_usernames, cred.all_alternative_usernames);
  EXPECT_EQ(form.date_created, cred.date_created);
  EXPECT_EQ(form.date_last_used, cred.date_last_used);
  EXPECT_EQ(form.date_last_filled, cred.date_last_filled);
  EXPECT_EQ(form.date_password_modified, cred.date_password_modified);
  EXPECT_EQ(form.date_received, cred.date_received);
  EXPECT_EQ(form.blocked_by_user, cred.blocked_by_user);
  EXPECT_EQ(form.type, cred.type);
  EXPECT_EQ(form.times_used_in_html_form, cred.times_used_in_html_form);
  EXPECT_EQ(form.display_name, cred.display_name);
  EXPECT_EQ(form.icon_url, cred.icon_url);
  EXPECT_EQ(form.skip_zero_click, cred.skip_zero_click);
  EXPECT_EQ(form.generation_upload_status, cred.generation_upload_status);
  EXPECT_EQ(form.in_store, cred.in_store);
  EXPECT_EQ(form.moving_blocked_for_list, cred.moving_blocked_for_list);
  EXPECT_EQ(form.password_issues, cred.password_issues);
  EXPECT_EQ(form.notes, cred.notes);
  EXPECT_EQ(form.form_data.name(), cred.form_data.name());
  EXPECT_EQ(form.form_data.url(), cred.form_data.url());
  EXPECT_EQ(form.keychain_identifier, cred.keychain_identifier);
  EXPECT_EQ(form.sender_email, cred.sender_email);
  EXPECT_EQ(form.sender_name, cred.sender_name);
  EXPECT_EQ(form.sharing_notification_displayed,
            cred.sharing_notification_displayed);
  EXPECT_EQ(form.sender_profile_image_url, cred.sender_profile_image_url);
  EXPECT_EQ(form.actor_login_approved, cred.actor_login_approved);
}

}  // namespace

TEST(PasswordFormConvertersTest, ToPasswordForm_AllFields) {
  StoredCredential cred = CreateFullStoredCredential();
  PasswordForm form = ToPasswordForm(cred);
  ExpectEqual(form, cred);
}

TEST(PasswordFormConvertersTest, ToPasswordForm_Defaults) {
  StoredCredential cred;
  PasswordForm form = ToPasswordForm(cred);
  ExpectEqual(form, cred);
}

TEST(PasswordFormConvertersTest, FromPasswordForm_AllFields) {
  PasswordForm form = CreateFullPasswordForm();
  StoredCredential cred = FromPasswordForm(form);
  ExpectEqual(form, cred);
}

TEST(PasswordFormConvertersTest, FromPasswordForm_Defaults) {
  PasswordForm form;
  StoredCredential cred = FromPasswordForm(form);
  ExpectEqual(form, cred);
}

}  // namespace password_manager
