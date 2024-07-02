// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/credential_manager_utils.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

class CredentialManagerUtilsTest : public testing::Test {
 protected:
  url::Origin origin_{url::Origin::Create(GURL("https://example.test/"))};
  GURL icon_{"https://fast-cdn.test/icon.png"};
};

TEST_F(CredentialManagerUtilsTest, CreatePasswordFormEmpty) {
  CredentialInfo info;
  std::unique_ptr<PasswordForm> form;

  // Empty CredentialInfo -> nullptr.
  form = CreatePasswordFormFromCredentialInfo(info, origin_);
  EXPECT_EQ(nullptr, form.get());
}

TEST_F(CredentialManagerUtilsTest, CreatePasswordFormFederation) {
  CredentialInfo info;
  std::unique_ptr<PasswordForm> form;

  info.id = u"id";
  info.name = u"name";
  info.icon = icon_;
  info.federation = url::SchemeHostPort(GURL("https://federation.test/"));
  info.type = CredentialType::CREDENTIAL_TYPE_FEDERATED;

  form = CreatePasswordFormFromCredentialInfo(info, origin_);
  ASSERT_NE(nullptr, form.get());

  EXPECT_EQ(PasswordForm::Type::kApi, form->type);
  EXPECT_EQ(info.icon, form->icon_url);
  EXPECT_EQ(info.name, form->display_name);
  EXPECT_EQ(origin_.GetURL(), form->url);
  EXPECT_EQ(PasswordForm::Scheme::kHtml, form->scheme);

  // Federated credentials have empty passwords, non-empty federation_origins,
  // and funky signon realms.
  EXPECT_EQ(info.federation, form->federation_origin);
  EXPECT_EQ(std::u16string(), form->password_value);
  EXPECT_EQ("federation://example.test/federation.test", form->signon_realm);
}

TEST_F(CredentialManagerUtilsTest, CreatePasswordFormLocal) {
  CredentialInfo info;
  std::unique_ptr<PasswordForm> form;

  info.id = u"id";
  info.name = u"name";
  info.icon = icon_;
  info.password = u"password";
  info.type = CredentialType::CREDENTIAL_TYPE_PASSWORD;

  form = CreatePasswordFormFromCredentialInfo(info, origin_);
  ASSERT_NE(nullptr, form.get());

  EXPECT_EQ(info.icon, form->icon_url);
  EXPECT_EQ(info.name, form->display_name);
  EXPECT_EQ(origin_.GetURL().spec(), form->url);
  EXPECT_EQ(PasswordForm::Scheme::kHtml, form->scheme);

  // Local credentials have empty federation_origins, non-empty passwords, and
  // a signon realm that matches the origin.
  EXPECT_EQ(form->federation_origin, url::SchemeHostPort());
  EXPECT_EQ(info.password, form->password_value);
  EXPECT_EQ(origin_.GetURL().spec(), form->signon_realm);
}

}  // namespace password_manager
