// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/values_test_util.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

namespace {

const char kCollate[] = "collate";
const char kDisplayName[] = "display_name";
const char kDpi[] = "dpi";
const char kId[] = "id";
const char kIsDefault[] = "is_default";
const char kMediaSizes[] = "media_sizes";
const char kPagesPerSheet[] = "Pages per sheet";
const char kPaperType[] = "Paper Type";
const char kPrinter[] = "printer";
const char kResetToDefault[] = "reset_to_default";
const char kValue[] = "value";
const char kVendorCapability[] = "vendor_capability";

base::DictionaryValue GetCapabilitiesFull() {
  base::DictionaryValue printer;

  base::Value::ListStorage list_media;
  list_media.push_back(base::Value("Letter"));
  list_media.push_back(base::Value("A4"));
  printer.SetKey(kMediaSizes, base::Value(list_media));

  base::Value::ListStorage list_dpi;
  list_dpi.push_back(base::Value(300));
  list_dpi.push_back(base::Value(600));

  base::Value options(base::Value::Type::DICTIONARY);
  options.SetKey(kOptionKey, base::Value(list_dpi));
  printer.SetKey(kDpi, std::move(options));

  printer.SetKey(kCollate, base::Value(true));

  base::Value::ListStorage pages_per_sheet;
  for (int i = 1; i <= 8; i *= 2) {
    base::Value option(base::Value::Type::DICTIONARY);
    option.SetKey(kDisplayName, base::Value(std::to_string(i)));
    option.SetKey(kValue, base::Value(i));
    if (i == 1)
      option.SetKey(kIsDefault, base::Value(true));
    pages_per_sheet.push_back(std::move(option));
  }
  base::Value pages_per_sheet_option(base::Value::Type::DICTIONARY);
  pages_per_sheet_option.SetKey(kOptionKey, base::Value(pages_per_sheet));
  base::Value pages_per_sheet_capability(base::Value::Type::DICTIONARY);
  pages_per_sheet_capability.SetKey(kDisplayName, base::Value(kPagesPerSheet));
  pages_per_sheet_capability.SetKey(kId, base::Value(kPagesPerSheet));
  pages_per_sheet_capability.SetKey(kTypeKey, base::Value(kSelectString));
  pages_per_sheet_capability.SetKey(kSelectCapKey,
                                    std::move(pages_per_sheet_option));

  base::Value::ListStorage paper_types;
  base::Value option1(base::Value::Type::DICTIONARY);
  option1.SetKey(kDisplayName, base::Value("Plain"));
  option1.SetKey(kValue, base::Value("Plain"));
  option1.SetKey(kIsDefault, base::Value(true));
  base::Value option2(base::Value::Type::DICTIONARY);
  option2.SetKey(kDisplayName, base::Value("Photo"));
  option2.SetKey(kValue, base::Value("Photo"));
  paper_types.push_back(std::move(option1));
  paper_types.push_back(std::move(option2));
  base::Value paper_type_option(base::Value::Type::DICTIONARY);
  paper_type_option.SetKey(kOptionKey, base::Value(paper_types));
  base::Value paper_type_capability(base::Value::Type::DICTIONARY);
  paper_type_capability.SetKey(kDisplayName, base::Value(kPaperType));
  paper_type_capability.SetKey(kId, base::Value(kPaperType));
  paper_type_capability.SetKey(kTypeKey, base::Value(kSelectString));
  paper_type_capability.SetKey(kSelectCapKey, std::move(paper_type_option));

  base::Value::ListStorage vendor_capabilities;
  vendor_capabilities.push_back(std::move(pages_per_sheet_capability));
  vendor_capabilities.push_back(std::move(paper_type_capability));
  printer.SetKey(kVendorCapability, base::Value(vendor_capabilities));

  return printer;
}

base::Value ValidList(const base::Value* list) {
  auto out_list = list->Clone();
  out_list.EraseListValueIf([](const base::Value& v) { return v.is_none(); });
  return out_list;
}

bool HasValidEntry(const base::Value* list) {
  return list && !list->GetListDeprecated().empty() &&
         !ValidList(list).GetListDeprecated().empty();
}

void CompareStringKeys(const base::Value& expected,
                       const base::Value& actual,
                       base::StringPiece key) {
  EXPECT_EQ(*(expected.FindKeyOfType(key, base::Value::Type::STRING)),
            *(actual.FindKeyOfType(key, base::Value::Type::STRING)));
}

void ValidateList(const base::Value* list_out, const base::Value* input_list) {
  auto input_list_valid = ValidList(input_list);
  ASSERT_EQ(list_out->GetListDeprecated().size(),
            input_list_valid.GetListDeprecated().size());
  for (size_t index = 0; index < list_out->GetListDeprecated().size();
       index++) {
    EXPECT_EQ(list_out->GetListDeprecated()[index],
              input_list_valid.GetListDeprecated()[index]);
  }
}

void ValidateMedia(const base::Value* printer_out,
                   const base::Value* expected_list) {
  const base::Value* media_out =
      printer_out->FindKeyOfType(kMediaSizes, base::Value::Type::LIST);
  if (!HasValidEntry(expected_list)) {
    EXPECT_FALSE(media_out);
    return;
  }
  ValidateList(media_out, expected_list);
}

void ValidateDpi(const base::Value* printer_out,
                 const base::Value* expected_dpi) {
  const base::Value* dpi_option_out =
      printer_out->FindKeyOfType(kDpi, base::Value::Type::DICTIONARY);
  if (!expected_dpi) {
    EXPECT_FALSE(dpi_option_out);
    return;
  }
  const base::Value* dpi_list =
      expected_dpi->FindKeyOfType(kOptionKey, base::Value::Type::LIST);
  if (!HasValidEntry(dpi_list)) {
    EXPECT_FALSE(dpi_option_out);
    return;
  }
  ASSERT_TRUE(dpi_option_out);
  const base::Value* dpi_list_out =
      dpi_option_out->FindKeyOfType(kOptionKey, base::Value::Type::LIST);
  ASSERT_TRUE(dpi_list_out);
  ValidateList(dpi_list_out, dpi_list);
}

void ValidateCollate(const base::Value* printer_out) {
  const base::Value* collate_out =
      printer_out->FindKeyOfType(kCollate, base::Value::Type::BOOLEAN);
  ASSERT_TRUE(collate_out);
}

void ValidateVendorCaps(const base::Value* printer_out,
                        const base::Value* input_vendor_caps) {
  const base::Value* vendor_capability_out =
      printer_out->FindKeyOfType(kVendorCapability, base::Value::Type::LIST);
  if (!HasValidEntry(input_vendor_caps)) {
    ASSERT_FALSE(vendor_capability_out);
    return;
  }

  ASSERT_TRUE(vendor_capability_out);
  size_t index = 0;
  base::Value::ConstListView output_list =
      vendor_capability_out->GetListDeprecated();
  for (const auto& input_entry : input_vendor_caps->GetListDeprecated()) {
    if (!HasValidEntry(
            input_entry
                .FindKeyOfType(kSelectCapKey, base::Value::Type::DICTIONARY)
                ->FindKeyOfType(kOptionKey, base::Value::Type::LIST))) {
      continue;
    }
    CompareStringKeys(input_entry, output_list[index], kDisplayName);
    CompareStringKeys(input_entry, output_list[index], kId);
    CompareStringKeys(input_entry, output_list[index], kTypeKey);
    const base::Value* select_cap = output_list[index].FindKeyOfType(
        kSelectCapKey, base::Value::Type::DICTIONARY);
    ASSERT_TRUE(select_cap);
    const base::Value* list =
        select_cap->FindKeyOfType(kOptionKey, base::Value::Type::LIST);
    ASSERT_TRUE(list);
    ValidateList(
        list,
        input_entry.FindKeyOfType(kSelectCapKey, base::Value::Type::DICTIONARY)
            ->FindKeyOfType(kOptionKey, base::Value::Type::LIST));
    index++;
  }
}

void ValidatePrinter(const base::Value* cdd_out,
                     const base::DictionaryValue& printer) {
  const base::Value* printer_out =
      cdd_out->FindKeyOfType(kPrinter, base::Value::Type::DICTIONARY);
  ASSERT_TRUE(printer_out);

  const base::Value* media =
      printer.FindKeyOfType(kMediaSizes, base::Value::Type::LIST);
  ValidateMedia(printer_out, media);

  const base::Value* dpi_dict =
      printer.FindKeyOfType(kDpi, base::Value::Type::DICTIONARY);
  ValidateDpi(printer_out, dpi_dict);
  ValidateCollate(printer_out);

  const base::Value* capabilities_list =
      printer.FindKeyOfType(kVendorCapability, base::Value::Type::LIST);
  ValidateVendorCaps(printer_out, capabilities_list);
}

}  // namespace

using PrintPreviewUtilsTest = testing::Test;

TEST_F(PrintPreviewUtilsTest, FullCddPassthrough) {
  base::DictionaryValue printer = GetCapabilitiesFull();
  base::DictionaryValue cdd;
  cdd.SetKey(kPrinter, printer.Clone());
  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(&cdd_out, printer);
}

TEST_F(PrintPreviewUtilsTest, FilterBadList) {
  base::DictionaryValue printer = GetCapabilitiesFull();
  printer.RemoveKey(kMediaSizes);
  base::Value::ListStorage list_media;
  list_media.push_back(base::Value());
  list_media.push_back(base::Value());
  printer.SetKey(kMediaSizes, base::Value(list_media));
  base::DictionaryValue cdd;
  cdd.SetKey(kPrinter, printer.Clone());
  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(&cdd_out, printer);
}

TEST_F(PrintPreviewUtilsTest, FilterBadOptionOneElement) {
  base::DictionaryValue printer = GetCapabilitiesFull();
  printer.RemoveKey(kDpi);
  base::Value options(base::Value::Type::DICTIONARY);
  base::Value::ListStorage list_dpi;
  list_dpi.push_back(base::Value());
  list_dpi.push_back(base::Value(600));
  options.SetKey(kOptionKey, base::Value(list_dpi));
  printer.SetKey(kDpi, std::move(options));
  base::DictionaryValue cdd;
  cdd.SetKey(kPrinter, printer.Clone());
  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(&cdd_out, printer);
}

TEST_F(PrintPreviewUtilsTest, FilterBadOptionAllElement) {
  base::DictionaryValue printer = GetCapabilitiesFull();
  printer.RemoveKey(kDpi);
  base::Value options(base::Value::Type::DICTIONARY);
  base::Value::ListStorage list_dpi;
  list_dpi.push_back(base::Value());
  list_dpi.push_back(base::Value());
  options.SetKey(kOptionKey, base::Value(list_dpi));
  printer.SetKey(kDpi, std::move(options));
  base::DictionaryValue cdd;
  cdd.SetKey(kPrinter, printer.Clone());
  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(&cdd_out, printer);
}

TEST_F(PrintPreviewUtilsTest, FilterBadVendorCapabilityAllElement) {
  base::DictionaryValue printer = GetCapabilitiesFull();
  base::Value* select_cap_0 =
      printer.FindKeyOfType(kVendorCapability, base::Value::Type::LIST)
          ->GetListDeprecated()[0]
          .FindKeyOfType(kSelectCapKey, base::Value::Type::DICTIONARY);
  select_cap_0->RemoveKey(kOptionKey);
  base::Value::ListStorage option_list;
  option_list.push_back(base::Value());
  option_list.push_back(base::Value());
  select_cap_0->SetKey(kOptionKey, base::Value(option_list));
  base::DictionaryValue cdd;
  cdd.SetKey(kPrinter, printer.Clone());
  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(&cdd_out, printer);
}

TEST_F(PrintPreviewUtilsTest, FilterBadVendorCapabilityOneElement) {
  base::DictionaryValue printer = GetCapabilitiesFull();
  base::Value* vendor_dictionary =
      printer.FindKeyOfType(kVendorCapability, base::Value::Type::LIST)
          ->GetListDeprecated()[0]
          .FindKeyOfType(kSelectCapKey, base::Value::Type::DICTIONARY);
  vendor_dictionary->RemoveKey(kOptionKey);
  base::Value::ListStorage pages_per_sheet;
  for (int i = 1; i <= 8; i *= 2) {
    if (i == 2) {
      pages_per_sheet.push_back(base::Value());
      continue;
    }
    base::Value option(base::Value::Type::DICTIONARY);
    option.SetKey(kDisplayName, base::Value(std::to_string(i)));
    option.SetKey(kValue, base::Value(i));
    if (i == 1)
      option.SetKey(kIsDefault, base::Value(true));
    pages_per_sheet.push_back(std::move(option));
  }
  vendor_dictionary->SetKey(kOptionKey, base::Value(pages_per_sheet));

  base::DictionaryValue cdd;
  cdd.SetKey(kPrinter, printer.Clone());
  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(&cdd_out, printer);
}

bool GetDpiResetToDefault(base::Value cdd) {
  base::Value* printer =
      cdd.FindKeyOfType(kPrinter, base::Value::Type::DICTIONARY);
  base::Value* dpi =
      printer->FindKeyOfType(kDpi, base::Value::Type::DICTIONARY);
  absl::optional<bool> reset_to_default = dpi->FindBoolKey(kResetToDefault);
  EXPECT_TRUE(reset_to_default);
  return *reset_to_default;
}

TEST_F(PrintPreviewUtilsTest, CddResetToDefault) {
  base::DictionaryValue printer = GetCapabilitiesFull();
  base::Value* dpi_dict =
      printer.FindKeyOfType(kDpi, base::Value::Type::DICTIONARY);

  base::DictionaryValue cdd;
  dpi_dict->SetKey(kResetToDefault, base::Value(true));
  cdd.SetKey(kPrinter, printer.Clone());
  auto cdd_out = ValidateCddForPrintPreview(cdd.Clone());
  ValidatePrinter(&cdd_out, printer);
  EXPECT_TRUE(GetDpiResetToDefault(std::move(cdd_out)));

  dpi_dict->SetKey(kResetToDefault, base::Value(false));
  cdd.SetKey(kPrinter, printer.Clone());
  cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(&cdd_out, printer);
  EXPECT_FALSE(GetDpiResetToDefault(std::move(cdd_out)));
}

}  // namespace printing
