// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/credential_manager_types.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

class CredentialManagerTypesTest : public testing::Test {
 public:
  CredentialManagerTypesTest()
      : origin_(GURL("https://example.test/")),
        icon_(GURL("https://fast-cdn.test/icon.png")),
        federation_(url::Origin::Create(GURL("https://federation.test/"))) {}

 protected:
  GURL origin_;
  GURL icon_;
  url::Origin federation_;
};

TEST_F(CredentialManagerTypesTest, CreatePasswordFormEmpty) {
  CredentialInfo info;
  std::unique_ptr<autofill::PasswordForm> form;

  // Empty CredentialInfo -> nullptr.
  form = CreatePasswordFormFromCredentialInfo(info, origin_);
  EXPECT_EQ(nullptr, form.get());
}

TEST_F(CredentialManagerTypesTest, CreatePasswordFormFederation) {
  CredentialInfo info;
  std::unique_ptr<autofill::PasswordForm> form;

  info.id = base::ASCIIToUTF16("id");
  info.name = base::ASCIIToUTF16("name");
  info.icon = icon_;
  info.federation = federation_;
  info.type = CredentialType::CREDENTIAL_TYPE_FEDERATED;

  form = CreatePasswordFormFromCredentialInfo(info, origin_);
  ASSERT_NE(nullptr, form.get());

  EXPECT_EQ(autofill::PasswordForm::Type::kApi, form->type);
  EXPECT_EQ(info.icon, form->icon_url);
  EXPECT_EQ(info.name, form->display_name);
  EXPECT_EQ(origin_, form->origin);
  EXPECT_EQ(autofill::PasswordForm::Scheme::kHtml, form->scheme);

  // Federated credentials have empty passwords, non-empty federation_origins,
  // and funky signon realms.
  EXPECT_EQ(info.federation, form->federation_origin);
  EXPECT_EQ(base::string16(), form->password_value);
  EXPECT_EQ("federation://example.test/federation.test", form->signon_realm);
}

TEST_F(CredentialManagerTypesTest, CreatePasswordFormLocal) {
  CredentialInfo info;
  std::unique_ptr<autofill::PasswordForm> form;

  info.id = base::ASCIIToUTF16("id");
  info.name = base::ASCIIToUTF16("name");
  info.icon = icon_;
  info.password = base::ASCIIToUTF16("password");
  info.type = CredentialType::CREDENTIAL_TYPE_PASSWORD;

  form = CreatePasswordFormFromCredentialInfo(info, origin_);
  ASSERT_NE(nullptr, form.get());

  EXPECT_EQ(info.icon, form->icon_url);
  EXPECT_EQ(info.name, form->display_name);
  EXPECT_EQ(origin_, form->origin);
  EXPECT_EQ(autofill::PasswordForm::Scheme::kHtml, form->scheme);

  // Local credentials have empty federation_origins, non-empty passwords, and
  // a signon realm that matches the origin.
  EXPECT_TRUE(form->federation_origin.opaque());
  EXPECT_EQ(info.password, form->password_value);
  EXPECT_EQ(origin_.spec(), form->signon_realm);
}

}  // namespace password_manager
