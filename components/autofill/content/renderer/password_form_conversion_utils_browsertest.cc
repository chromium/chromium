// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>

#include "base/strings/stringprintf.h"
#include "components/autofill/content/renderer/password_form_conversion_utils.h"
#include "content/public/test/render_view_test.h"
#include "google_apis/gaia/gaia_urls.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_local_frame.h"

using blink::WebFormElement;
using blink::WebLocalFrame;
using blink::WebVector;

namespace autofill {

namespace {

// A builder to produce HTML code for a password form composed of the desired
// number and kinds of username and password fields.
class PasswordFormBuilder {
 public:
  // Creates a builder to start composing a new form. The form will have the
  // specified |action| URL.
  explicit PasswordFormBuilder(const char* action) {
    base::StringAppendF(
        &html_, "<FORM name=\"Test\" action=\"%s\" method=\"post\">", action);
  }

  // Appends a new text-type field at the end of the form, having the specified
  // |name_and_id|, |value|, and |autocomplete| attributes. The |autocomplete|
  // argument can take two special values, namely:
  //  1.) nullptr, causing no autocomplete attribute to be added,
  //  2.) "", causing an empty attribute (i.e. autocomplete="") to be added.
  void AddTextField(const char* name_and_id,
                    const char* value,
                    const char* autocomplete) {
    std::string autocomplete_attribute(autocomplete ?
        base::StringPrintf("autocomplete=\"%s\"", autocomplete) : "");
    base::StringAppendF(
        &html_,
        "<INPUT type=\"text\" name=\"%s\" id=\"%s\" value=\"%s\" %s/>",
        name_and_id, name_and_id, value, autocomplete_attribute.c_str());
  }

  // Appends a new password-type field at the end of the form, having the
  // specified |name_and_id|, |value|, and |autocomplete| attributes. Special
  // values for |autocomplete| are the same as in AddTextField.
  void AddPasswordField(const char* name_and_id,
                        const char* value,
                        const char* autocomplete) {
    std::string autocomplete_attribute(
        autocomplete ? base::StringPrintf("autocomplete=\"%s\"", autocomplete)
                     : "");
    base::StringAppendF(
        &html_,
        "<INPUT type=\"password\" name=\"%s\" id=\"%s\" value=\"%s\" %s/>",
        name_and_id, name_and_id, value, autocomplete_attribute.c_str());
  }

  // Appends a new hidden-type field at the end of the form, having the
  // specified |name_and_id| and |value| attributes.
  void AddHiddenField(const char* name_and_id, const char* value) {
    base::StringAppendF(
        &html_, "<INPUT type=\"hidden\" name=\"%s\" id=\"%s\" value=\"%s\" />",
        name_and_id, name_and_id, value);
  }

  // Returns the HTML code for the form containing the fields that have been
  // added so far.
  std::string ProduceHTML() const { return html_ + "</FORM>"; }

 private:
  std::string html_;

  DISALLOW_COPY_AND_ASSIGN(PasswordFormBuilder);
};

class PasswordFormConversionUtilsTest : public content::RenderViewTest {
 public:
  PasswordFormConversionUtilsTest() = default;
  ~PasswordFormConversionUtilsTest() override = default;

 protected:
  // Loads the given |html| and retrieves the sole WebFormElement from it.
  void LoadWebFormFromHTML(const std::string& html,
                           WebFormElement* form,
                           const char* origin) {
    if (origin)
      LoadHTMLWithUrlOverride(html.c_str(), origin);
    else
      LoadHTML(html.c_str());

    GetFirstForm(form);
  }

  void TearDown() override {
    content::RenderViewTest::TearDown();
  }

 private:
  void GetFirstForm(WebFormElement* form) {
    WebLocalFrame* frame = GetMainFrame();
    ASSERT_TRUE(frame);

    WebVector<WebFormElement> forms;
    frame->GetDocument().Forms(forms);
    ASSERT_LE(1U, forms.size());

    *form = forms[0];
  }

  DISALLOW_COPY_AND_ASSIGN(PasswordFormConversionUtilsTest);
};

}  // namespace

TEST_F(PasswordFormConversionUtilsTest, IsGaiaReauthFormIgnored) {
  struct TestCase {
    const char* origin;
    struct KeyValue {
      KeyValue() : name(nullptr), value(nullptr) {}
      KeyValue(const char* new_name, const char* new_value)
          : name(new_name), value(new_value) {}
      const char* name;
      const char* value;
    } hidden_fields[2];
    bool expected_form_is_reauth;
  } cases[] = {
      // A common password form is parsed successfully.
      {"https://example.com",
       {TestCase::KeyValue(), TestCase::KeyValue()},
       false},
      // A common password form, even if it appears on a GAIA reauth url,
      // is parsed successfully.
      {"https://accounts.google.com",
       {TestCase::KeyValue(), TestCase::KeyValue()},
       false},
      // Not a transactional reauth.
      {"https://accounts.google.com",
       {TestCase::KeyValue("continue", "https://passwords.google.com/settings"),
        TestCase::KeyValue()},
       false},
      // A reauth form that is not for a password site is parsed successfuly.
      {"https://accounts.google.com",
       {TestCase::KeyValue("continue", "https://mail.google.com"),
        TestCase::KeyValue("rart", "")},
       false},
      // A reauth form for a password site is recognised as such.
      {"https://accounts.google.com",
       {TestCase::KeyValue("continue", "https://passwords.google.com"),
        TestCase::KeyValue("rart", "")},
       true},
      // Path, params or fragment in "continue" should not have influence.
      {"https://accounts.google.com",
       {TestCase::KeyValue("continue",
                           "https://passwords.google.com/path?param=val#frag"),
        TestCase::KeyValue("rart", "")},
       true},
      // Password site is inaccesible via HTTP, but because of HSTS the
      // following link should still continue to https://passwords.google.com.
      {"https://accounts.google.com",
       {TestCase::KeyValue("continue", "http://passwords.google.com"),
        TestCase::KeyValue("rart", "")},
       true},
      // Make sure testing sites are disabled as well.
      {"https://accounts.google.com",
       {TestCase::KeyValue(
            "continue",
            "https://passwords-ac-testing.corp.google.com/settings"),
        TestCase::KeyValue("rart", "")},
       true},
      // Specifying default port doesn't change anything.
      {"https://accounts.google.com",
       {TestCase::KeyValue("continue", "passwords.google.com:443"),
        TestCase::KeyValue("rart", "")},
       true},
      // Fully qualified domain should work as well.
      {"https://accounts.google.com",
       {TestCase::KeyValue("continue",
                           "https://passwords.google.com./settings"),
        TestCase::KeyValue("rart", "")},
       true},
      // A correctly looking form, but on a different page.
      {"https://google.com",
       {TestCase::KeyValue("continue", "https://passwords.google.com"),
        TestCase::KeyValue("rart", "")},
       false},
  };

  for (TestCase& test_case : cases) {
    SCOPED_TRACE(testing::Message("origin=")
                 << test_case.origin
                 << ", hidden_fields[0]=" << test_case.hidden_fields[0].name
                 << "/" << test_case.hidden_fields[0].value
                 << ", hidden_fields[1]=" << test_case.hidden_fields[1].name
                 << "/" << test_case.hidden_fields[1].value
                 << ", expected_form_is_reauth="
                 << test_case.expected_form_is_reauth);
    std::unique_ptr<PasswordFormBuilder> builder(new PasswordFormBuilder(""));
    builder->AddTextField("username", "", nullptr);
    builder->AddPasswordField("password", "", nullptr);
    for (TestCase::KeyValue& hidden_field : test_case.hidden_fields) {
      if (hidden_field.name)
        builder->AddHiddenField(hidden_field.name, hidden_field.value);
    }
    std::string html = builder->ProduceHTML();
    WebFormElement form;
    LoadWebFormFromHTML(html, &form, test_case.origin);
    EXPECT_EQ(test_case.expected_form_is_reauth,
              IsGaiaReauthenticationForm(form));
  }
}

TEST_F(PasswordFormConversionUtilsTest, IsGaiaWithSkipSavePasswordForm) {
  struct TestCase {
    const char* origin;
    bool expected_form_has_skip_save_password;
  } cases[] = {
      // A common password form is parsed successfully.
      {"https://example.com", false},
      // A common GAIA sign-in page, with no skip save password argument.
      {"https://accounts.google.com", false},
      // A common GAIA sign-in page, with "0" skip save password argument.
      {"https://accounts.google.com/?ssp=0", false},
      // A common GAIA sign-in page, with skip save password argument.
      {"https://accounts.google.com/?ssp=1", true},
      // The Gaia page that is used to start a Chrome sign-in flow when Desktop
      // Identity Consistency is enable.
      {GaiaUrls::GetInstance()->signin_chrome_sync_dice().spec().c_str(), true},
  };

  for (TestCase& test_case : cases) {
    SCOPED_TRACE(testing::Message("origin=")
                 << test_case.origin
                 << ", expected_form_has_skip_save_password="
                 << test_case.expected_form_has_skip_save_password);
    std::unique_ptr<PasswordFormBuilder> builder(new PasswordFormBuilder(""));
    builder->AddTextField("username", "", nullptr);
    builder->AddPasswordField("password", "", nullptr);
    std::string html = builder->ProduceHTML();
    WebFormElement form;
    LoadWebFormFromHTML(html, &form, test_case.origin);
    EXPECT_EQ(test_case.expected_form_has_skip_save_password,
              IsGaiaWithSkipSavePasswordForm(form));
  }
}

}  // namespace autofill
