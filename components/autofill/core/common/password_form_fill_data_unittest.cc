// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/password_form_fill_data.h"

#include <map>
#include <memory>
#include <utility>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

// Tests that the when there is a single preferred match, and no extra
// matches, the PasswordFormFillData is filled in correctly.
TEST(PasswordFormFillDataTest, TestSinglePreferredMatch) {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.origin = GURL("https://foo.com/");
  form_on_page.action = GURL("https://foo.com/login");
  form_on_page.username_element = ASCIIToUTF16("username");
  form_on_page.username_value = ASCIIToUTF16("test@gmail.com");
  form_on_page.password_element = ASCIIToUTF16("password");
  form_on_page.password_value = ASCIIToUTF16("test");
  form_on_page.submit_element = ASCIIToUTF16("");
  form_on_page.signon_realm = "https://foo.com/";
  form_on_page.preferred = false;
  form_on_page.scheme = PasswordForm::Scheme::kHtml;

  // Create an exact match in the database.
  PasswordForm preferred_match;
  preferred_match.origin = GURL("https://foo.com/");
  preferred_match.action = GURL("https://foo.com/login");
  preferred_match.username_element = ASCIIToUTF16("username");
  preferred_match.username_value = ASCIIToUTF16("test@gmail.com");
  preferred_match.password_element = ASCIIToUTF16("password");
  preferred_match.password_value = ASCIIToUTF16("test");
  preferred_match.submit_element = ASCIIToUTF16("");
  preferred_match.signon_realm = "https://foo.com/";
  preferred_match.preferred = true;
  preferred_match.scheme = PasswordForm::Scheme::kHtml;

  std::vector<const PasswordForm*> matches;

  PasswordFormFillData result(form_on_page, matches, preferred_match, true);

  // |wait_for_username| should reflect the |wait_for_username| argument passed
  // to the constructor, which in this case is true.
  EXPECT_TRUE(result.wait_for_username);
  // The preferred realm should be empty since it's the same as the realm of
  // the form.
  EXPECT_EQ(std::string(), result.preferred_realm);

  PasswordFormFillData result2(form_on_page, matches, preferred_match, false);

  // |wait_for_username| should reflect the |wait_for_username| argument passed
  // to the constructor, which in this case is false.
  EXPECT_FALSE(result2.wait_for_username);
}

// Tests that constructing a PasswordFormFillData behaves correctly when there
// is a preferred match that was found using public suffix matching, an
// additional result that also used public suffix matching, and a third result
// that was found without using public suffix matching.
TEST(PasswordFormFillDataTest, TestPublicSuffixDomainMatching) {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.origin = GURL("https://foo.com/");
  form_on_page.action = GURL("https://foo.com/login");
  form_on_page.username_element = ASCIIToUTF16("username");
  form_on_page.username_value = ASCIIToUTF16("test@gmail.com");
  form_on_page.password_element = ASCIIToUTF16("password");
  form_on_page.password_value = ASCIIToUTF16("test");
  form_on_page.submit_element = ASCIIToUTF16("");
  form_on_page.signon_realm = "https://foo.com/";
  form_on_page.preferred = false;
  form_on_page.scheme = PasswordForm::Scheme::kHtml;

  // Create a match from the database that matches using public suffix.
  PasswordForm preferred_match;
  preferred_match.origin = GURL("https://mobile.foo.com/");
  preferred_match.action = GURL("https://mobile.foo.com/login");
  preferred_match.username_element = ASCIIToUTF16("username");
  preferred_match.username_value = ASCIIToUTF16("test@gmail.com");
  preferred_match.password_element = ASCIIToUTF16("password");
  preferred_match.password_value = ASCIIToUTF16("test");
  preferred_match.submit_element = ASCIIToUTF16("");
  preferred_match.signon_realm = "https://foo.com/";
  preferred_match.is_public_suffix_match = true;
  preferred_match.preferred = true;
  preferred_match.scheme = PasswordForm::Scheme::kHtml;

  // Create a match that matches exactly, so |is_public_suffix_match| has a
  // default value false.
  PasswordForm exact_match;
  exact_match.origin = GURL("https://foo.com/");
  exact_match.action = GURL("https://foo.com/login");
  exact_match.username_element = ASCIIToUTF16("username");
  exact_match.username_value = ASCIIToUTF16("test1@gmail.com");
  exact_match.password_element = ASCIIToUTF16("password");
  exact_match.password_value = ASCIIToUTF16("test");
  exact_match.submit_element = ASCIIToUTF16("");
  exact_match.signon_realm = "https://foo.com/";
  exact_match.preferred = false;
  exact_match.scheme = PasswordForm::Scheme::kHtml;

  // Create a match that was matched using public suffix, so
  // |is_public_suffix_match| == true.
  PasswordForm public_suffix_match;
  public_suffix_match.origin = GURL("https://foo.com/");
  public_suffix_match.action = GURL("https://foo.com/login");
  public_suffix_match.username_element = ASCIIToUTF16("username");
  public_suffix_match.username_value = ASCIIToUTF16("test2@gmail.com");
  public_suffix_match.password_element = ASCIIToUTF16("password");
  public_suffix_match.password_value = ASCIIToUTF16("test");
  public_suffix_match.submit_element = ASCIIToUTF16("");
  public_suffix_match.is_public_suffix_match = true;
  public_suffix_match.signon_realm = "https://foo.com/";
  public_suffix_match.preferred = false;
  public_suffix_match.scheme = PasswordForm::Scheme::kHtml;

  // Add one exact match and one public suffix match.
  std::vector<const PasswordForm*> matches = {&exact_match,
                                              &public_suffix_match};

  PasswordFormFillData result(form_on_page, matches, preferred_match, true);
  EXPECT_TRUE(result.wait_for_username);
  // The preferred realm should match the signon realm from the
  // preferred match so the user can see where the result came from.
  EXPECT_EQ(preferred_match.signon_realm, result.preferred_realm);

  // The realm of the exact match should be empty.
  PasswordFormFillData::LoginCollection::const_iterator iter =
      result.additional_logins.find(exact_match.username_value);
  EXPECT_EQ(std::string(), iter->second.realm);

  // The realm of the public suffix match should be set to the original signon
  // realm so the user can see where the result came from.
  iter = result.additional_logins.find(public_suffix_match.username_value);
  EXPECT_EQ(iter->second.realm, public_suffix_match.signon_realm);
}

// Tests that the constructing a PasswordFormFillData behaves correctly when
// there is a preferred match that was found using affiliation based matching,
// an additional result that also used affiliation based matching, and a third
// result that was found without using affiliation based matching.
TEST(PasswordFormFillDataTest, TestAffiliationMatch) {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.origin = GURL("https://foo.com/");
  form_on_page.action = GURL("https://foo.com/login");
  form_on_page.username_element = ASCIIToUTF16("username");
  form_on_page.username_value = ASCIIToUTF16("test@gmail.com");
  form_on_page.password_element = ASCIIToUTF16("password");
  form_on_page.password_value = ASCIIToUTF16("test");
  form_on_page.submit_element = ASCIIToUTF16("");
  form_on_page.signon_realm = "https://foo.com/";
  form_on_page.preferred = false;
  form_on_page.scheme = PasswordForm::Scheme::kHtml;

  // Create a match from the database that matches using affiliation.
  PasswordForm preferred_match;
  preferred_match.origin = GURL("android://hash@foo.com/");
  preferred_match.username_value = ASCIIToUTF16("test@gmail.com");
  preferred_match.password_value = ASCIIToUTF16("test");
  preferred_match.signon_realm = "android://hash@foo.com/";
  preferred_match.is_affiliation_based_match = true;
  preferred_match.preferred = true;

  // Create a match that matches exactly, so |is_affiliation_based_match| has a
  // default value false.
  PasswordForm exact_match;
  exact_match.origin = GURL("https://foo.com/");
  exact_match.action = GURL("https://foo.com/login");
  exact_match.username_element = ASCIIToUTF16("username");
  exact_match.username_value = ASCIIToUTF16("test1@gmail.com");
  exact_match.password_element = ASCIIToUTF16("password");
  exact_match.password_value = ASCIIToUTF16("test");
  exact_match.submit_element = ASCIIToUTF16("");
  exact_match.signon_realm = "https://foo.com/";
  exact_match.preferred = false;
  exact_match.scheme = PasswordForm::Scheme::kHtml;

  // Create a match that was matched using public suffix, so
  // |is_public_suffix_match| == true.
  PasswordForm affiliated_match;
  affiliated_match.origin = GURL("android://hash@foo1.com/");
  affiliated_match.username_value = ASCIIToUTF16("test2@gmail.com");
  affiliated_match.password_value = ASCIIToUTF16("test");
  affiliated_match.is_affiliation_based_match = true;
  affiliated_match.signon_realm = "https://foo1.com/";
  affiliated_match.preferred = false;
  affiliated_match.scheme = PasswordForm::Scheme::kHtml;

  // Add one exact match and one affiliation based match.
  std::vector<const PasswordForm*> matches = {&exact_match, &affiliated_match};

  PasswordFormFillData result(form_on_page, matches, preferred_match, false);
  EXPECT_FALSE(result.wait_for_username);
  // The preferred realm should match the signon realm from the
  // preferred match so the user can see where the result came from.
  EXPECT_EQ(preferred_match.signon_realm, result.preferred_realm);

  // The realm of the exact match should be empty.
  PasswordFormFillData::LoginCollection::const_iterator iter =
      result.additional_logins.find(exact_match.username_value);
  EXPECT_EQ(std::string(), iter->second.realm);

  // The realm of the affiliation based match should be set to the original
  // signon realm so the user can see where the result came from.
  iter = result.additional_logins.find(affiliated_match.username_value);
  EXPECT_EQ(iter->second.realm, affiliated_match.signon_realm);
}

// Tests that renderer ids are passed correctly.
TEST(PasswordFormFillDataTest, RendererIDs) {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.origin = GURL("https://foo.com/");
  form_on_page.action = GURL("https://foo.com/login");
  form_on_page.username_element = ASCIIToUTF16("username");
  form_on_page.password_element = ASCIIToUTF16("password");
  form_on_page.username_may_use_prefilled_placeholder = true;

  // Create an exact match in the database.
  PasswordForm preferred_match = form_on_page;
  preferred_match.username_value = ASCIIToUTF16("test@gmail.com");
  preferred_match.password_value = ASCIIToUTF16("test");
  preferred_match.preferred = true;

  // Set renderer id related fields.
  FormData form_data;
  form_data.unique_renderer_id = 42;
  form_data.is_form_tag = true;
  form_on_page.form_data = form_data;
  form_on_page.has_renderer_ids = true;
  form_on_page.username_element_renderer_id = 123;
  form_on_page.password_element_renderer_id = 456;

  PasswordFormFillData result(form_on_page, {}, preferred_match, true);

  EXPECT_EQ(form_data.unique_renderer_id, result.form_renderer_id);
  EXPECT_EQ(form_on_page.has_renderer_ids, result.has_renderer_ids);
  EXPECT_EQ(form_on_page.username_element_renderer_id,
            result.username_field.unique_renderer_id);
  EXPECT_EQ(form_on_page.password_element_renderer_id,
            result.password_field.unique_renderer_id);
  EXPECT_TRUE(result.username_may_use_prefilled_placeholder);
}

// Tests that nor username nor password fields are set when password element is
// not found.
TEST(PasswordFormFillDataTest, NoPasswordElement) {
  // Create the current form on the page.
  PasswordForm form_on_page;
  form_on_page.origin = GURL("https://foo.com/");
  form_on_page.has_renderer_ids = true;
  form_on_page.username_element_renderer_id = 123;
  // Set no password element.
  form_on_page.password_element_renderer_id =
      std::numeric_limits<uint32_t>::max();
  form_on_page.new_password_element_renderer_id = 456;

  // Create an exact match in the database.
  PasswordForm preferred_match = form_on_page;
  preferred_match.username_value = ASCIIToUTF16("test@gmail.com");
  preferred_match.password_value = ASCIIToUTF16("test");
  preferred_match.preferred = true;

  FormData form_data;
  form_data.unique_renderer_id = 42;
  form_data.is_form_tag = true;
  form_on_page.form_data = form_data;

  PasswordFormFillData result(form_on_page, {} /* matches */, preferred_match,
                              true);

  // Check that nor username nor password fields are set.
  EXPECT_EQ(true, result.has_renderer_ids);
  EXPECT_EQ(std::numeric_limits<uint32_t>::max(),
            result.username_field.unique_renderer_id);
  EXPECT_EQ(std::numeric_limits<uint32_t>::max(),
            result.password_field.unique_renderer_id);
}

}  // namespace autofill
