// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/test_autofill_types.mojom.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/signatures_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using mojom::PasswordFormFieldPredictionType;

const std::vector<const char*> kOptions = {"Option1", "Option2", "Option3",
                                           "Option4"};
namespace {

template <typename T>
bool EquivalentData(const T& a, const T& b) {
  typename T::IdentityComparator less;
  return !less(a, b) && !less(b, a);
}

void CreateTestFieldDataPredictions(const std::string& signature,
                                    FormFieldDataPredictions* field_predict) {
  test::CreateTestSelectField("TestLabel", "TestName", "TestValue", kOptions,
                              kOptions, 4, &field_predict->field);
  field_predict->signature = signature;
  field_predict->heuristic_type = "TestSignature";
  field_predict->server_type = "TestServerType";
  field_predict->overall_type = "TestOverallType";
  field_predict->parseable_name = "TestParseableName";
  field_predict->section = "TestSection";
}

void CreateTestPasswordFormFillData(PasswordFormFillData* fill_data) {
  fill_data->form_renderer_id = 1234;
  fill_data->origin = GURL("https://foo.com/");
  fill_data->action = GURL("https://foo.com/login");
  test::CreateTestSelectField("TestUsernameFieldLabel", "TestUsernameFieldName",
                              "TestUsernameFieldValue", kOptions, kOptions, 4,
                              &fill_data->username_field);
  test::CreateTestSelectField("TestPasswordFieldLabel", "TestPasswordFieldName",
                              "TestPasswordFieldValue", kOptions, kOptions, 4,
                              &fill_data->password_field);
  fill_data->preferred_realm = "https://foo.com/";
  fill_data->uses_account_store = true;

  base::string16 name;
  PasswordAndMetadata pr;
  name = base::ASCIIToUTF16("Tom");
  pr.password = base::ASCIIToUTF16("Tom_Password");
  pr.realm = "https://foo.com/";
  pr.uses_account_store = false;
  fill_data->additional_logins[name] = pr;
  name = base::ASCIIToUTF16("Jerry");
  pr.password = base::ASCIIToUTF16("Jerry_Password");
  pr.realm = "https://bar.com/";
  pr.uses_account_store = true;
  fill_data->additional_logins[name] = pr;

  fill_data->wait_for_username = true;
}

void CreateTestPasswordForm(PasswordForm* form) {
  form->scheme = PasswordForm::Scheme::kHtml;
  form->signon_realm = "https://foo.com/";
  form->origin = GURL("https://foo.com/");
  form->action = GURL("https://foo.com/login");
  form->affiliated_web_realm = "https://foo.com/";
  form->submit_element = base::ASCIIToUTF16("test_submit");
  form->username_element = base::ASCIIToUTF16("username");
  form->username_marked_by_site = true;
  form->username_value = base::ASCIIToUTF16("test@gmail.com");
  form->all_possible_usernames.push_back(ValueElementPair(
      base::ASCIIToUTF16("Jerry_1"), base::ASCIIToUTF16("id1")));
  form->all_possible_usernames.push_back(ValueElementPair(
      base::ASCIIToUTF16("Jerry_2"), base::ASCIIToUTF16("id2")));
  form->all_possible_passwords.push_back(
      ValueElementPair(base::ASCIIToUTF16("pass1"), base::ASCIIToUTF16("el1")));
  form->all_possible_passwords.push_back(
      ValueElementPair(base::ASCIIToUTF16("pass2"), base::ASCIIToUTF16("el2")));
  form->form_has_autofilled_value = true;
  form->password_element = base::ASCIIToUTF16("password");
  form->password_value = base::ASCIIToUTF16("test");
  form->new_password_element = base::ASCIIToUTF16("new_password");
  form->new_password_value = base::ASCIIToUTF16("new_password_value");
  form->new_password_marked_by_site = false;
  form->new_password_element = base::ASCIIToUTF16("confirmation_password");
  form->preferred = false;
  form->date_created = AutofillClock::Now();
  form->date_synced = AutofillClock::Now();
  form->blacklisted_by_user = false;
  form->type = PasswordForm::Type::kGenerated;
  form->times_used = 999;
  test::CreateTestAddressFormData(&form->form_data);
  form->generation_upload_status =
      PasswordForm::GenerationUploadStatus::kPositiveSignalSent;
  form->display_name = base::ASCIIToUTF16("test display name");
  form->icon_url = GURL("https://foo.com/icon.png");
  form->federation_origin = url::Origin::Create(GURL("http://wwww.google.com"));
  form->skip_zero_click = false;
  form->was_parsed_using_autofill_predictions = false;
  form->is_public_suffix_match = true;
  form->is_affiliation_based_match = true;
  form->submission_event =
      mojom::SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;
}

void CreatePasswordGenerationUIData(
    password_generation::PasswordGenerationUIData* data) {
  data->bounds = gfx::RectF(1, 1, 200, 100);
  data->max_length = 20;
  data->generation_element = base::ASCIIToUTF16("generation_element");
  data->text_direction = base::i18n::RIGHT_TO_LEFT;
  data->is_generation_element_password_type = false;
  CreateTestPasswordForm(&data->password_form);
}

void CheckEqualPasswordFormFillData(const PasswordFormFillData& expected,
                                    const PasswordFormFillData& actual) {
  EXPECT_EQ(expected.form_renderer_id, actual.form_renderer_id);
  EXPECT_EQ(expected.origin, actual.origin);
  EXPECT_EQ(expected.action, actual.action);
  EXPECT_TRUE(EquivalentData(expected.username_field, actual.username_field));
  EXPECT_TRUE(EquivalentData(expected.password_field, actual.password_field));
  EXPECT_EQ(expected.preferred_realm, actual.preferred_realm);
  EXPECT_EQ(expected.uses_account_store, actual.uses_account_store);

  {
    EXPECT_EQ(expected.additional_logins.size(),
              actual.additional_logins.size());
    auto iter1 = expected.additional_logins.begin();
    auto end1 = expected.additional_logins.end();
    auto iter2 = actual.additional_logins.begin();
    auto end2 = actual.additional_logins.end();
    for (; iter1 != end1 && iter2 != end2; ++iter1, ++iter2) {
      EXPECT_EQ(iter1->first, iter2->first);
      EXPECT_EQ(iter1->second.password, iter2->second.password);
      EXPECT_EQ(iter1->second.realm, iter2->second.realm);
      EXPECT_EQ(iter1->second.uses_account_store,
                iter2->second.uses_account_store);
    }
    ASSERT_EQ(iter1, end1);
    ASSERT_EQ(iter2, end2);
  }

  EXPECT_EQ(expected.wait_for_username, actual.wait_for_username);
}

void CheckEqualPassPasswordGenerationUIData(
    const password_generation::PasswordGenerationUIData& expected,
    const password_generation::PasswordGenerationUIData& actual) {
  EXPECT_EQ(expected.bounds, actual.bounds);
  EXPECT_EQ(expected.max_length, actual.max_length);
  EXPECT_EQ(expected.generation_element, actual.generation_element);
  EXPECT_EQ(expected.is_generation_element_password_type,
            actual.is_generation_element_password_type);
  EXPECT_EQ(expected.text_direction, actual.text_direction);
  EXPECT_EQ(expected.password_form, actual.password_form);
}

}  // namespace

class AutofillTypeTraitsTestImpl : public testing::Test,
                                   public mojom::TypeTraitsTest {
 public:
  AutofillTypeTraitsTestImpl() {}

  mojo::PendingRemote<mojom::TypeTraitsTest> GetTypeTraitsTestRemote() {
    mojo::PendingRemote<mojom::TypeTraitsTest> remote;
    receivers_.Add(this, remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  // mojom::TypeTraitsTest:
  void PassFormData(const FormData& s, PassFormDataCallback callback) override {
    std::move(callback).Run(s);
  }

  void PassFormFieldData(const FormFieldData& s,
                         PassFormFieldDataCallback callback) override {
    std::move(callback).Run(s);
  }

  void PassFormDataPredictions(
      const FormDataPredictions& s,
      PassFormDataPredictionsCallback callback) override {
    std::move(callback).Run(s);
  }

  void PassFormFieldDataPredictions(
      const FormFieldDataPredictions& s,
      PassFormFieldDataPredictionsCallback callback) override {
    std::move(callback).Run(s);
  }

  void PassPasswordFormFillData(
      const PasswordFormFillData& s,
      PassPasswordFormFillDataCallback callback) override {
    std::move(callback).Run(s);
  }

  void PassPasswordFormGenerationData(
      const PasswordFormGenerationData& s,
      PassPasswordFormGenerationDataCallback callback) override {
    std::move(callback).Run(s);
  }

  void PassPasswordGenerationUIData(
      const password_generation::PasswordGenerationUIData& s,
      PassPasswordGenerationUIDataCallback callback) override {
    std::move(callback).Run(s);
  }

  void PassPasswordForm(const PasswordForm& s,
                        PassPasswordFormCallback callback) override {
    std::move(callback).Run(s);
  }

 private:
  base::test::TaskEnvironment task_environment_;

  mojo::ReceiverSet<TypeTraitsTest> receivers_;
};

void ExpectFormFieldData(const FormFieldData& expected,
                         base::OnceClosure closure,
                         const FormFieldData& passed) {
  EXPECT_TRUE(EquivalentData(expected, passed));
  EXPECT_EQ(expected.value, passed.value);
  EXPECT_EQ(expected.typed_value, passed.typed_value);
  std::move(closure).Run();
}

void ExpectFormData(const FormData& expected,
                    base::OnceClosure closure,
                    const FormData& passed) {
  EXPECT_TRUE(EquivalentData(expected, passed));
  std::move(closure).Run();
}

void ExpectFormFieldDataPredictions(const FormFieldDataPredictions& expected,
                                    base::OnceClosure closure,
                                    const FormFieldDataPredictions& passed) {
  EXPECT_EQ(expected, passed);
  std::move(closure).Run();
}

void ExpectFormDataPredictions(const FormDataPredictions& expected,
                               base::OnceClosure closure,
                               const FormDataPredictions& passed) {
  EXPECT_EQ(expected, passed);
  std::move(closure).Run();
}

void ExpectPasswordFormFillData(const PasswordFormFillData& expected,
                                base::OnceClosure closure,
                                const PasswordFormFillData& passed) {
  CheckEqualPasswordFormFillData(expected, passed);
  std::move(closure).Run();
}

void ExpectPasswordFormGenerationData(
    const PasswordFormGenerationData& expected,
    base::OnceClosure closure,
    const PasswordFormGenerationData& passed) {
  EXPECT_EQ(expected.new_password_renderer_id, passed.new_password_renderer_id);
  EXPECT_EQ(expected.confirmation_password_renderer_id,
            passed.confirmation_password_renderer_id);
  std::move(closure).Run();
}

void ExpectPasswordGenerationUIData(
    const password_generation::PasswordGenerationUIData& expected,
    base::OnceClosure closure,
    const password_generation::PasswordGenerationUIData& passed) {
  CheckEqualPassPasswordGenerationUIData(expected, passed);
  std::move(closure).Run();
}

void ExpectPasswordForm(const PasswordForm& expected,
                        base::OnceClosure closure,
                        const PasswordForm& passed) {
  EXPECT_EQ(expected, passed);
  std::move(closure).Run();
}

TEST_F(AutofillTypeTraitsTestImpl, PassFormFieldData) {
  FormFieldData input;
  test::CreateTestSelectField("TestLabel", "TestName", "TestValue", kOptions,
                              kOptions, 4, &input);
  // Set other attributes to check if they are passed correctly.
  input.id_attribute = base::ASCIIToUTF16("id");
  input.name_attribute = base::ASCIIToUTF16("name");
  input.autocomplete_attribute = "on";
  input.placeholder = base::ASCIIToUTF16("placeholder");
  input.css_classes = base::ASCIIToUTF16("class1");
  input.aria_label = base::ASCIIToUTF16("aria label");
  input.aria_description = base::ASCIIToUTF16("aria description");
  input.max_length = 12345;
  input.is_autofilled = true;
  input.check_status = FormFieldData::CheckStatus::kChecked;
  input.should_autocomplete = true;
  input.role = FormFieldData::RoleAttribute::kPresentation;
  input.text_direction = base::i18n::RIGHT_TO_LEFT;
  input.properties_mask = FieldPropertiesFlags::HAD_FOCUS;
  input.typed_value = base::ASCIIToUTF16("TestTypedValue");

  base::RunLoop loop;
  mojo::Remote<mojom::TypeTraitsTest> remote(GetTypeTraitsTestRemote());
  remote->PassFormFieldData(
      input, base::BindOnce(&ExpectFormFieldData, input, loop.QuitClosure()));
  loop.Run();
}

TEST_F(AutofillTypeTraitsTestImpl, PassFormData) {
  FormData input;
  test::CreateTestAddressFormData(&input);
  input.username_predictions = {1, 13, 2};
  input.button_titles.push_back(
      std::make_pair(base::ASCIIToUTF16("Sign-up"),
                     mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE));

  base::RunLoop loop;
  mojo::Remote<mojom::TypeTraitsTest> remote(GetTypeTraitsTestRemote());
  remote->PassFormData(
      input, base::BindOnce(&ExpectFormData, input, loop.QuitClosure()));
  loop.Run();
}

TEST_F(AutofillTypeTraitsTestImpl, PassFormFieldDataPredictions) {
  FormFieldDataPredictions input;
  CreateTestFieldDataPredictions("TestSignature", &input);

  base::RunLoop loop;
  mojo::Remote<mojom::TypeTraitsTest> remote(GetTypeTraitsTestRemote());
  remote->PassFormFieldDataPredictions(
      input, base::BindOnce(&ExpectFormFieldDataPredictions, input,
                            loop.QuitClosure()));
  loop.Run();
}

TEST_F(AutofillTypeTraitsTestImpl, PassFormDataPredictions) {
  FormDataPredictions input;
  test::CreateTestAddressFormData(&input.data);
  input.signature = "TestSignature";

  FormFieldDataPredictions field_predict;
  CreateTestFieldDataPredictions("Tom", &field_predict);
  input.fields.push_back(field_predict);
  CreateTestFieldDataPredictions("Jerry", &field_predict);
  input.fields.push_back(field_predict);
  CreateTestFieldDataPredictions("NoOne", &field_predict);
  input.fields.push_back(field_predict);

  base::RunLoop loop;
  mojo::Remote<mojom::TypeTraitsTest> remote(GetTypeTraitsTestRemote());
  remote->PassFormDataPredictions(
      input,
      base::BindOnce(&ExpectFormDataPredictions, input, loop.QuitClosure()));
  loop.Run();
}

TEST_F(AutofillTypeTraitsTestImpl, PassPasswordFormFillData) {
  PasswordFormFillData input;
  CreateTestPasswordFormFillData(&input);

  base::RunLoop loop;
  mojo::Remote<mojom::TypeTraitsTest> remote(GetTypeTraitsTestRemote());
  remote->PassPasswordFormFillData(
      input,
      base::BindOnce(&ExpectPasswordFormFillData, input, loop.QuitClosure()));
  loop.Run();
}

TEST_F(AutofillTypeTraitsTestImpl, PasswordFormGenerationData) {
  PasswordFormGenerationData input;
  input.new_password_renderer_id = 1234u,
  input.confirmation_password_renderer_id = 5789u;

  base::RunLoop loop;
  mojo::Remote<mojom::TypeTraitsTest> remote(GetTypeTraitsTestRemote());
  remote->PassPasswordFormGenerationData(
      input, base::BindOnce(&ExpectPasswordFormGenerationData, input,
                            loop.QuitClosure()));
  loop.Run();
}

TEST_F(AutofillTypeTraitsTestImpl, PassPasswordGenerationUIData) {
  password_generation::PasswordGenerationUIData input;
  CreatePasswordGenerationUIData(&input);

  base::RunLoop loop;
  mojo::Remote<mojom::TypeTraitsTest> remote(GetTypeTraitsTestRemote());
  remote->PassPasswordGenerationUIData(
      input, base::BindOnce(&ExpectPasswordGenerationUIData, input,
                            loop.QuitClosure()));
  loop.Run();
}

TEST_F(AutofillTypeTraitsTestImpl, PassPasswordForm) {
  PasswordForm input;
  CreateTestPasswordForm(&input);

  base::RunLoop loop;
  mojo::Remote<mojom::TypeTraitsTest> remote(GetTypeTraitsTestRemote());
  remote->PassPasswordForm(
      input, base::BindOnce(&ExpectPasswordForm, input, loop.QuitClosure()));
  loop.Run();
}

}  // namespace autofill
