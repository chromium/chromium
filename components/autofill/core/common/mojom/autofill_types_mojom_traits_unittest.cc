// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/html_field_types.h"
#include "components/autofill/core/common/mojom/test_autofill_types.mojom.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

bool operator==(const PasswordAndMetadata& lhs,
                const PasswordAndMetadata& rhs) {
  return lhs.username == rhs.username && lhs.password == rhs.password &&
         lhs.realm == rhs.realm &&
         lhs.uses_account_store == rhs.uses_account_store;
}

const std::vector<const char*> kOptions = {"Option1", "Option2", "Option3",
                                           "Option4"};
namespace {

void CreateTestFieldDataPredictions(const std::string& signature,
                                    FormFieldDataPredictions* field_predict) {
  field_predict->host_form_signature = "TestHostFormSignature";
  field_predict->signature = signature;
  field_predict->heuristic_type = "TestHeuristicType";
  field_predict->server_type = "TestServerType";
  field_predict->overall_type = "TestOverallType";
  field_predict->parseable_name = "TestParseableName";
  field_predict->section = "TestSection";
}

void CreateTestPasswordFormFillData(PasswordFormFillData* fill_data) {
  fill_data->form_renderer_id = autofill::FormRendererId(1234);
  fill_data->url = GURL("https://foo.com/");
  fill_data->preferred_login.username = u"TestUsernameFieldValue";
  fill_data->username_element_renderer_id = test::MakeFieldRendererId();
  fill_data->preferred_login.password = u"TestPasswordFieldValue";
  fill_data->password_element_renderer_id = test::MakeFieldRendererId();
  fill_data->preferred_login.realm = "https://foo.com/";
  fill_data->preferred_login.uses_account_store = true;

  PasswordAndMetadata pr;
  pr.password = u"Tom_Password";
  pr.realm = "https://foo.com/";
  pr.uses_account_store = false;
  pr.username = u"Tom";
  fill_data->additional_logins.push_back(pr);
  pr.password = u"Jerry_Password";
  pr.realm = "https://bar.com/";
  pr.uses_account_store = true;
  pr.username = u"Jerry";
  fill_data->additional_logins.push_back(pr);

  fill_data->wait_for_username = true;
}

void CreatePasswordGenerationUIData(
    password_generation::PasswordGenerationUIData* data) {
  data->bounds = gfx::RectF(1, 1, 200, 100);
  data->max_length = 20;
  data->generation_element = u"generation_element";
  data->text_direction = base::i18n::RIGHT_TO_LEFT;
  data->is_generation_element_password_type = false;
  test::CreateTestAddressFormData(&data->form_data);
}

void CheckEqualPasswordFormFillData(const PasswordFormFillData& expected,
                                    const PasswordFormFillData& actual) {
  EXPECT_EQ(expected.form_renderer_id, actual.form_renderer_id);
  EXPECT_EQ(expected.username_element_renderer_id,
            actual.username_element_renderer_id);
  EXPECT_EQ(expected.password_element_renderer_id,
            actual.password_element_renderer_id);
  EXPECT_EQ(expected.preferred_login, actual.preferred_login);
  EXPECT_EQ(expected.additional_logins, actual.additional_logins);
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
  EXPECT_TRUE(test::WithoutUnserializedData(expected.form_data)
                  .SameFormAs(actual.form_data));
}

}  // namespace

class AutofillTypeTraitsTestImpl : public testing::Test,
                                   public mojom::TypeTraitsTest {
 public:
  AutofillTypeTraitsTestImpl() = default;

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

  void PassSection(const Section& s, PassSectionCallback callback) override {
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

 private:
  base::test::TaskEnvironment task_environment_;
  test::AutofillEnvironment autofill_environment_;

  mojo::ReceiverSet<TypeTraitsTest> receivers_;
};

void ExpectFormFieldData(const FormFieldData& expected,
                         base::OnceClosure closure,
                         const FormFieldData& passed) {
  EXPECT_TRUE(passed.host_frame.is_empty());
  EXPECT_TRUE(FormFieldData::DeepEqual(test::WithoutUnserializedData(expected),
                                       passed));
  EXPECT_EQ(expected.value, passed.value);
  EXPECT_EQ(expected.user_input, passed.user_input);
  std::move(closure).Run();
}

void ExpectFormData(const FormData& expected,
                    base::OnceClosure closure,
                    const FormData& passed) {
  EXPECT_TRUE(passed.host_frame.is_empty());
  EXPECT_TRUE(
      FormData::DeepEqual(test::WithoutUnserializedData(expected), passed));
  std::move(closure).Run();
}

void ExpectFormFieldDataPredictions(const FormFieldDataPredictions& expected,
                                    base::OnceClosure closure,
                                    const FormFieldDataPredictions& passed) {
  EXPECT_EQ(expected, passed);
  std::move(closure).Run();
}

void ExpectFormDataPredictions(FormDataPredictions expected,
                               base::OnceClosure closure,
                               const FormDataPredictions& passed) {
  expected.data = test::WithoutUnserializedData(expected.data);
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

// Test all Section::SectionPrefix states.
class AutofillTypeTraitsTestImplSectionTest
    : public AutofillTypeTraitsTestImpl,
      public testing::WithParamInterface<Section> {
 public:
  const Section& section() const { return GetParam(); }
};

TEST_P(AutofillTypeTraitsTestImplSectionTest, PassSection) {
  base::RunLoop loop;
  mojo::Remote<mojom::TypeTraitsTest> remote(GetTypeTraitsTestRemote());
  remote->PassSection(
      section(),
      base::BindOnce(
          [](const Section& a, base::OnceClosure closure, const Section& b) {
            EXPECT_EQ(a, b);
            std::move(closure).Run();
          },
          section(), loop.QuitClosure()));
  loop.Run();
}

std::vector<Section> SectionTestCases() {
  std::vector<Section> test_cases;
  Section s;
  // Default.
  test_cases.push_back(s);

  // Autocomplete.
  s = Section::FromAutocomplete(
      {.section = "autocomplete_section", .mode = HtmlFieldMode::kBilling});
  test_cases.push_back(s);

  // FieldIdentifier.
  base::flat_map<LocalFrameToken, size_t> frame_token_ids;
  FormFieldData field;
  field.name = u"from_field_name";
  field.host_frame = test::MakeLocalFrameToken();
  field.unique_renderer_id = FieldRendererId(123);
  s = Section::FromFieldIdentifier(field, frame_token_ids);
  test_cases.push_back(s);

  return test_cases;
}

INSTANTIATE_TEST_SUITE_P(All,
                         AutofillTypeTraitsTestImplSectionTest,
                         testing::ValuesIn(SectionTestCases()));

TEST_F(AutofillTypeTraitsTestImpl, PassFormFieldData) {
  FormFieldData input;
  test::CreateTestSelectField("TestLabel", "TestName", "TestValue", kOptions,
                              kOptions, &input);
  // Set other attributes to check if they are passed correctly.
  input.host_frame = test::MakeLocalFrameToken();
  input.unique_renderer_id = FieldRendererId(1234);
  input.id_attribute = u"id";
  input.name_attribute = u"name";
  input.autocomplete_attribute = "on";
  input.parsed_autocomplete =
      AutocompleteParsingResult{.section = "autocomplete_section",
                                .mode = HtmlFieldMode::kShipping,
                                .field_type = HtmlFieldType::kAddressLine1};
  input.placeholder = u"placeholder";
  input.css_classes = u"class1";
  input.aria_label = u"aria label";
  input.aria_description = u"aria description";
  input.max_length = 12345;
  input.is_autofilled = true;
  input.check_status = FormFieldData::CheckStatus::kChecked;
  input.should_autocomplete = true;
  input.role = FormFieldData::RoleAttribute::kPresentation;
  input.text_direction = base::i18n::RIGHT_TO_LEFT;
  input.properties_mask = FieldPropertiesFlags::kHadFocus;
  input.user_input = u"TestTypedValue";
  input.bounds = gfx::RectF(1, 2, 10, 100);
  base::flat_map<LocalFrameToken, size_t> frame_token_ids;
  input.section = Section::FromAutocomplete(
      {.section = "autocomplete_section", .mode = HtmlFieldMode::kShipping});

  EXPECT_FALSE(input.host_frame.is_empty());
  base::RunLoop loop;
  mojo::Remote<mojom::TypeTraitsTest> remote(GetTypeTraitsTestRemote());
  remote->PassFormFieldData(
      input, base::BindOnce(&ExpectFormFieldData, input, loop.QuitClosure()));
  loop.Run();
}

TEST_F(AutofillTypeTraitsTestImpl, PassDataListFormFieldData) {
  // Basically copied from PassFormFieldData and replaced Select with Datalist.
  FormFieldData input;
  test::CreateTestDatalistField("DatalistLabel", "DatalistName",
                                "DatalistValue", kOptions, kOptions, &input);
  // Set other attributes to check if they are passed correctly.
  input.host_frame = test::MakeLocalFrameToken();
  input.unique_renderer_id = FieldRendererId(1234);
  input.id_attribute = u"id";
  input.name_attribute = u"name";
  input.autocomplete_attribute = "on";
  input.parsed_autocomplete = absl::nullopt;
  input.placeholder = u"placeholder";
  input.css_classes = u"class1";
  input.aria_label = u"aria label";
  input.aria_description = u"aria description";
  input.max_length = 12345;
  input.is_autofilled = true;
  input.check_status = FormFieldData::CheckStatus::kChecked;
  input.should_autocomplete = true;
  input.role = FormFieldData::RoleAttribute::kPresentation;
  input.text_direction = base::i18n::RIGHT_TO_LEFT;
  input.properties_mask = FieldPropertiesFlags::kHadFocus;
  input.user_input = u"TestTypedValue";
  input.bounds = gfx::RectF(1, 2, 10, 100);

  EXPECT_FALSE(input.host_frame.is_empty());
  base::RunLoop loop;
  mojo::Remote<mojom::TypeTraitsTest> remote(GetTypeTraitsTestRemote());
  remote->PassFormFieldData(
      input, base::BindOnce(&ExpectFormFieldData, input, loop.QuitClosure()));
  loop.Run();
}

TEST_F(AutofillTypeTraitsTestImpl, PassFormData) {
  FormData input;
  test::CreateTestAddressFormData(&input);
  input.username_predictions = {autofill::FieldRendererId(1),
                                autofill::FieldRendererId(13),
                                autofill::FieldRendererId(2)};
  input.button_titles.push_back(std::make_pair(
      u"Sign-up", mojom::ButtonTitleType::BUTTON_ELEMENT_SUBMIT_TYPE));

  EXPECT_FALSE(input.host_frame.is_empty());
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
  input.new_password_renderer_id = autofill::FieldRendererId(1234u),
  input.confirmation_password_renderer_id = autofill::FieldRendererId(5789u);

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

}  // namespace autofill
