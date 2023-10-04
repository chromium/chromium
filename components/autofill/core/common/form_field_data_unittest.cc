// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/form_field_data.h"

#include "base/i18n/rtl.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/autofill_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

void FillCommonFields(FormFieldData* data) {
  data->label = u"label";
  data->name = u"name";
  data->value = u"value";
  data->form_control_type = FormControlType::kInputPassword;
  data->autocomplete_attribute = "off";
  data->max_length = 200;
  data->is_autofilled = true;
  data->check_status = FormFieldData::CheckStatus::kChecked;
  data->is_focusable = true;
  data->should_autocomplete = false;
  data->text_direction = base::i18n::RIGHT_TO_LEFT;
  data->options = {{.value = u"First", .content = u"First"},
                   {.value = u"Second", .content = u"Second"}};
}

void FillVersion2Fields(FormFieldData* data) {
  data->role = FormFieldData::RoleAttribute::kPresentation;
}

void FillVersion3Fields(FormFieldData* data) {
  data->placeholder = u"placeholder";
}

void FillVersion5Fields(FormFieldData* data) {
  data->css_classes = u"class1 class2";
}

void FillVersion6Fields(FormFieldData* data) {
  data->properties_mask =
      FieldPropertiesFlags::kUserTyped | FieldPropertiesFlags::kHadFocus;
}

void FillVersion7Fields(FormFieldData* data) {
  data->id_attribute = u"id";
}

void FillVersion8Fields(FormFieldData* data) {
  data->name_attribute = u"name";
}

void WriteSection1(const FormFieldData& data, base::Pickle* pickle) {
  pickle->WriteString16(data.label);
  pickle->WriteString16(data.name);
  pickle->WriteString16(data.value);
  pickle->WriteString(FormControlTypeToString(data.form_control_type));
  pickle->WriteString(data.autocomplete_attribute);
  pickle->WriteUInt64(data.max_length);
  pickle->WriteBool(data.is_autofilled);
}

void WriteSection3(const FormFieldData& data, base::Pickle* pickle) {
  pickle->WriteBool(IsChecked(data.check_status));
  pickle->WriteBool(IsCheckable(data.check_status));
}

void WriteSection4(const FormFieldData& data, base::Pickle* pickle) {
  pickle->WriteInt(static_cast<int>(data.check_status));
}

void WriteSection5(const FormFieldData& data, base::Pickle* pickle) {
  pickle->WriteBool(data.is_focusable);
  pickle->WriteBool(data.should_autocomplete);
}

void WriteSection2(const FormFieldData& data, base::Pickle* pickle) {
  pickle->WriteInt(data.text_direction);
  pickle->WriteInt(static_cast<int>(data.options.size()));
  for (const auto& option : data.options)
    pickle->WriteString16(option.value);
  pickle->WriteInt(static_cast<int>(data.options.size()));
  for (const auto& option : data.options)
    pickle->WriteString16(option.content);
}

void WriteVersion9Specific(const FormFieldData& data, base::Pickle* pickle) {
  pickle->WriteInt(data.text_direction);
  pickle->WriteInt(static_cast<int>(data.options.size()));
  for (const SelectOption& option : data.options) {
    pickle->WriteString16(option.value);
    pickle->WriteString16(option.content);
  }
}

void WriteVersion2Specific(const FormFieldData& data, base::Pickle* pickle) {
  pickle->WriteInt(static_cast<int>(data.role));
}

void WriteVersion3Specific(const FormFieldData& data, base::Pickle* pickle) {
  pickle->WriteString16(data.placeholder);
}

void WriteVersion5Specific(const FormFieldData& data, base::Pickle* pickle) {
  pickle->WriteString16(data.css_classes);
}

void WriteVersion6Specific(const FormFieldData& data, base::Pickle* pickle) {
  pickle->WriteUInt32(data.properties_mask);
}

void WriteVersion7Specific(const FormFieldData& data, base::Pickle* pickle) {
  pickle->WriteString16(data.id_attribute);
}

void WriteVersion8Specific(const FormFieldData& data, base::Pickle* pickle) {
  pickle->WriteString16(data.name_attribute);
}

void SerializeInVersion1Format(const FormFieldData& data,
                               base::Pickle* pickle) {
  WriteSection1(data, pickle);
  WriteSection3(data, pickle);
  WriteSection5(data, pickle);
  WriteSection2(data, pickle);
}

void SerializeInVersion2Format(const FormFieldData& data,
                               base::Pickle* pickle) {
  WriteSection1(data, pickle);
  WriteSection3(data, pickle);
  WriteSection5(data, pickle);
  WriteVersion2Specific(data, pickle);
  WriteSection2(data, pickle);
}

void SerializeInVersion3Format(const FormFieldData& data,
                               base::Pickle* pickle) {
  WriteSection1(data, pickle);
  WriteSection3(data, pickle);
  WriteSection5(data, pickle);
  WriteVersion2Specific(data, pickle);
  WriteSection2(data, pickle);
  WriteVersion3Specific(data, pickle);
}

void SerializeInVersion4Format(const FormFieldData& data,
                               base::Pickle* pickle) {
  WriteSection1(data, pickle);
  WriteSection4(data, pickle);
  WriteSection5(data, pickle);
  WriteVersion2Specific(data, pickle);
  WriteSection2(data, pickle);
  WriteVersion3Specific(data, pickle);
}

void SerializeInVersion5Format(const FormFieldData& data,
                               base::Pickle* pickle) {
  WriteSection1(data, pickle);
  WriteSection4(data, pickle);
  WriteSection5(data, pickle);
  WriteVersion2Specific(data, pickle);
  WriteSection2(data, pickle);
  WriteVersion3Specific(data, pickle);
  WriteVersion5Specific(data, pickle);
}

void SerializeInVersion6Format(const FormFieldData& data,
                               base::Pickle* pickle) {
  WriteSection1(data, pickle);
  WriteSection4(data, pickle);
  WriteSection5(data, pickle);
  WriteVersion2Specific(data, pickle);
  WriteSection2(data, pickle);
  WriteVersion3Specific(data, pickle);
  WriteVersion5Specific(data, pickle);
  WriteVersion6Specific(data, pickle);
}

void SerializeInVersion7Format(const FormFieldData& data,
                               base::Pickle* pickle) {
  WriteSection1(data, pickle);
  WriteSection4(data, pickle);
  WriteSection5(data, pickle);
  WriteVersion2Specific(data, pickle);
  WriteSection2(data, pickle);
  WriteVersion3Specific(data, pickle);
  WriteVersion5Specific(data, pickle);
  WriteVersion6Specific(data, pickle);
  WriteVersion7Specific(data, pickle);
}

void SerializeInVersion8Format(const FormFieldData& data,
                               base::Pickle* pickle) {
  WriteSection1(data, pickle);
  WriteSection4(data, pickle);
  WriteSection5(data, pickle);
  WriteVersion2Specific(data, pickle);
  WriteSection2(data, pickle);
  WriteVersion3Specific(data, pickle);
  WriteVersion5Specific(data, pickle);
  WriteVersion6Specific(data, pickle);
  WriteVersion7Specific(data, pickle);
  WriteVersion8Specific(data, pickle);
}

void SerializeInVersion9Format(const FormFieldData& data,
                               base::Pickle* pickle) {
  WriteSection1(data, pickle);
  WriteSection4(data, pickle);
  WriteSection5(data, pickle);
  WriteVersion2Specific(data, pickle);
  WriteVersion9Specific(data, pickle);
  WriteVersion3Specific(data, pickle);
  WriteVersion5Specific(data, pickle);
  WriteVersion6Specific(data, pickle);
  WriteVersion7Specific(data, pickle);
  WriteVersion8Specific(data, pickle);
}

}  // namespace

TEST(FormFieldDataTest, SerializeAndDeserialize) {
  FormFieldData data;
  FillCommonFields(&data);
  FillVersion2Fields(&data);
  FillVersion3Fields(&data);
  FillVersion5Fields(&data);
  FillVersion6Fields(&data);
  FillVersion7Fields(&data);
  FillVersion8Fields(&data);

  base::Pickle pickle;
  SerializeFormFieldData(data, &pickle);

  base::PickleIterator iter(pickle);
  FormFieldData actual;
  EXPECT_TRUE(DeserializeFormFieldData(&iter, &actual));

  EXPECT_TRUE(actual.SameFieldAs(data));
}

TEST(FormFieldDataTest, DeserializeVersion1) {
  FormFieldData data;
  FillCommonFields(&data);

  base::Pickle pickle;
  pickle.WriteInt(1);
  SerializeInVersion1Format(data, &pickle);

  base::PickleIterator iter(pickle);
  FormFieldData actual;
  EXPECT_TRUE(DeserializeFormFieldData(&iter, &actual));

  EXPECT_TRUE(actual.SameFieldAs(data));
}

TEST(FormFieldDataTest, DeserializeVersion2) {
  FormFieldData data;
  FillCommonFields(&data);
  FillVersion2Fields(&data);

  base::Pickle pickle;
  pickle.WriteInt(2);
  SerializeInVersion2Format(data, &pickle);

  base::PickleIterator iter(pickle);
  FormFieldData actual;
  EXPECT_TRUE(DeserializeFormFieldData(&iter, &actual));

  EXPECT_TRUE(actual.SameFieldAs(data));
}

TEST(FormFieldDataTest, DeserializeVersion3) {
  FormFieldData data;
  FillCommonFields(&data);
  FillVersion2Fields(&data);
  FillVersion3Fields(&data);

  base::Pickle pickle;
  pickle.WriteInt(3);
  SerializeInVersion3Format(data, &pickle);

  base::PickleIterator iter(pickle);
  FormFieldData actual;
  EXPECT_TRUE(DeserializeFormFieldData(&iter, &actual));

  EXPECT_TRUE(actual.SameFieldAs(data));
}

TEST(FormFieldDataTest, DeserializeVersion4) {
  FormFieldData data;
  FillCommonFields(&data);
  FillVersion2Fields(&data);
  FillVersion3Fields(&data);

  base::Pickle pickle;
  pickle.WriteInt(4);
  SerializeInVersion4Format(data, &pickle);

  base::PickleIterator iter(pickle);
  FormFieldData actual;
  EXPECT_TRUE(DeserializeFormFieldData(&iter, &actual));

  EXPECT_TRUE(actual.SameFieldAs(data));
}

TEST(FormFieldDataTest, DeserializeVersion5) {
  FormFieldData data;
  FillCommonFields(&data);
  FillVersion2Fields(&data);
  FillVersion3Fields(&data);
  FillVersion5Fields(&data);

  base::Pickle pickle;
  pickle.WriteInt(5);
  SerializeInVersion5Format(data, &pickle);

  base::PickleIterator iter(pickle);
  FormFieldData actual;
  EXPECT_TRUE(DeserializeFormFieldData(&iter, &actual));

  EXPECT_TRUE(actual.SameFieldAs(data));
}

TEST(FormFieldDataTest, DeserializeVersion6) {
  FormFieldData data;
  FillCommonFields(&data);
  FillVersion2Fields(&data);
  FillVersion3Fields(&data);
  FillVersion5Fields(&data);
  FillVersion6Fields(&data);

  base::Pickle pickle;
  pickle.WriteInt(6);
  SerializeInVersion6Format(data, &pickle);

  base::PickleIterator iter(pickle);
  FormFieldData actual;
  EXPECT_TRUE(DeserializeFormFieldData(&iter, &actual));

  EXPECT_TRUE(actual.SameFieldAs(data));
}

TEST(FormFieldDataTest, DeserializeVersion7) {
  FormFieldData data;
  FillCommonFields(&data);
  FillVersion2Fields(&data);
  FillVersion3Fields(&data);
  FillVersion5Fields(&data);
  FillVersion6Fields(&data);
  FillVersion7Fields(&data);

  base::Pickle pickle;
  pickle.WriteInt(7);
  SerializeInVersion7Format(data, &pickle);

  base::PickleIterator iter(pickle);
  FormFieldData actual;
  EXPECT_TRUE(DeserializeFormFieldData(&iter, &actual));

  EXPECT_TRUE(actual.SameFieldAs(data));
}

TEST(FormFieldDataTest, DeserializeVersion8) {
  FormFieldData data;
  FillCommonFields(&data);
  FillVersion2Fields(&data);
  FillVersion3Fields(&data);
  FillVersion5Fields(&data);
  FillVersion6Fields(&data);
  FillVersion7Fields(&data);
  FillVersion8Fields(&data);

  base::Pickle pickle;
  pickle.WriteInt(8);
  SerializeInVersion8Format(data, &pickle);

  base::PickleIterator iter(pickle);
  FormFieldData actual;
  EXPECT_TRUE(DeserializeFormFieldData(&iter, &actual));

  EXPECT_TRUE(actual.SameFieldAs(data));
}

TEST(FormFieldDataTest, DeserializeVersion9) {
  FormFieldData data;
  FillCommonFields(&data);
  FillVersion2Fields(&data);
  FillVersion3Fields(&data);
  FillVersion5Fields(&data);
  FillVersion6Fields(&data);
  FillVersion7Fields(&data);
  FillVersion8Fields(&data);

  base::Pickle pickle;
  pickle.WriteInt(9);
  SerializeInVersion9Format(data, &pickle);

  base::PickleIterator iter(pickle);
  FormFieldData actual;
  EXPECT_TRUE(DeserializeFormFieldData(&iter, &actual));

  EXPECT_TRUE(actual.SameFieldAs(data));
}

// Verify that if the data isn't valid, the FormFieldData isn't populated
// during deserialization.
TEST(FormFieldDataTest, DeserializeBadData) {
  base::Pickle pickle;
  pickle.WriteInt(255);
  pickle.WriteString16(u"random");
  pickle.WriteString16(u"data");

  base::PickleIterator iter(pickle);
  FormFieldData actual;
  EXPECT_FALSE(DeserializeFormFieldData(&iter, &actual));
  FormFieldData empty;
  EXPECT_TRUE(actual.SameFieldAs(empty));
}

TEST(FormFieldDataTest, IsTextInputElement) {
  struct TestData {
    FormControlType form_control_type;
    bool expected;
  } test_data[] = {
      {FormControlType::kInputText, true},
      {FormControlType::kInputSearch, true},
      {FormControlType::kInputTelephone, true},
      {FormControlType::kInputUrl, true},
      {FormControlType::kInputEmail, true},
      {FormControlType::kInputPassword, true},
      {FormControlType::kInputNumber, true},
      {FormControlType::kSelectOne, false},
      {FormControlType::kInputCheckbox, false},
      {FormControlType::kTextArea, false},
  };

  for (const auto& test_case : test_data) {
    SCOPED_TRACE(testing::Message() << test_case.form_control_type);
    FormFieldData data;
    data.form_control_type = test_case.form_control_type;
    EXPECT_EQ(test_case.expected, data.IsTextInputElement());
  }
}

class FormFieldDataGetSelectionTest
    : public ::testing::TestWithParam<
          std::tuple<size_t, size_t, const char16_t*>> {
 public:
  size_t selection_start() const { return std::get<0>(GetParam()); }
  size_t selection_end() const { return std::get<1>(GetParam()); }
  std::u16string expectation() const { return std::get<2>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(FormFieldDataTest,
                         FormFieldDataGetSelectionTest,
                         ::testing::Values(std::make_tuple(0u, 0u, u""),
                                           std::make_tuple(0u, 3u, u"foo"),
                                           std::make_tuple(3u, 6u, u"bar"),
                                           std::make_tuple(6u, 6u, u""),
                                           std::make_tuple(0u, 100, u"foobar"),
                                           std::make_tuple(100u, 1000u, u"")));

TEST_P(FormFieldDataGetSelectionTest, GetSelection) {
  FormFieldData field;
  field.value = u"foobar";
  field.selection_start = selection_start();
  field.selection_end = selection_end();
  EXPECT_EQ(expectation(), field.GetSelection());
  EXPECT_EQ(expectation(), field.GetSelectionAsStringView());
}

}  // namespace autofill
