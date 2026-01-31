// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"

#include <memory>
#include <string_view>

#include "base/strings/string_number_conversions.h"
#include "base/test/values_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

namespace {

const char kCollate[] = "collate";
const char kDisplayName[] = "display_name";
const char kDpi[] = "dpi";
const char kHorizontalDpi[] = "horizontal_dpi";
const char kId[] = "id";
const char kIsDefault[] = "is_default";
const char kMediaHeight[] = "height_microns";
const char kMediaIsContinuousFeed[] = "is_continuous_feed";
const char kMediaSizeKey[] = "media_size";
const char kMediaWidth[] = "width_microns";
const char kMediaSizes[] = "media_sizes";
const char kPagesPerSheet[] = "Pages per sheet";
const char kPaperType[] = "Paper Type";
const char kPrinter[] = "printer";
const char kResetToDefault[] = "reset_to_default";
const char kValue[] = "value";
const char kVendorCapability[] = "vendor_capability";
const char kVerticalDpi[] = "vertical_dpi";

base::DictValue GetCapabilitiesFull() {
  base::DictValue printer;

  base::ListValue list_media;
  list_media.Append("Letter");
  list_media.Append("A4");
  printer.Set(kMediaSizes, std::move(list_media));

  base::DictValue dpi_300;
  dpi_300.Set(kHorizontalDpi, 300);
  dpi_300.Set(kVerticalDpi, 300);
  base::DictValue dpi_600;
  dpi_600.Set(kHorizontalDpi, 600);
  dpi_600.Set(kVerticalDpi, 600);
  base::ListValue list_dpi;
  list_dpi.Append(std::move(dpi_300));
  list_dpi.Append(std::move(dpi_600));

  base::DictValue options;
  options.Set(kOptionKey, std::move(list_dpi));
  printer.Set(kDpi, std::move(options));

  printer.Set(kCollate, true);

  base::ListValue pages_per_sheet;
  for (int i = 1; i <= 8; i *= 2) {
    base::DictValue option;
    option.Set(kDisplayName, base::NumberToString(i));
    option.Set(kValue, i);
    if (i == 1) {
      option.Set(kIsDefault, true);
    }
    pages_per_sheet.Append(std::move(option));
  }
  base::DictValue pages_per_sheet_option;
  pages_per_sheet_option.Set(kOptionKey, std::move(pages_per_sheet));
  base::DictValue pages_per_sheet_capability;
  pages_per_sheet_capability.Set(kDisplayName, kPagesPerSheet);
  pages_per_sheet_capability.Set(kId, kPagesPerSheet);
  pages_per_sheet_capability.Set(kTypeKey, kSelectString);
  pages_per_sheet_capability.Set(kSelectCapKey,
                                 std::move(pages_per_sheet_option));

  base::ListValue paper_types;
  base::DictValue option1;
  option1.Set(kDisplayName, "Plain");
  option1.Set(kValue, "Plain");
  option1.Set(kIsDefault, true);
  base::DictValue option2;
  option2.Set(kDisplayName, "Photo");
  option2.Set(kValue, "Photo");
  paper_types.Append(std::move(option1));
  paper_types.Append(std::move(option2));
  base::DictValue paper_type_option;
  paper_type_option.Set(kOptionKey, std::move(paper_types));
  base::DictValue paper_type_capability;
  paper_type_capability.Set(kDisplayName, kPaperType);
  paper_type_capability.Set(kId, kPaperType);
  paper_type_capability.Set(kTypeKey, kSelectString);
  paper_type_capability.Set(kSelectCapKey, std::move(paper_type_option));

  base::ListValue vendor_capabilities;
  vendor_capabilities.Append(std::move(pages_per_sheet_capability));
  vendor_capabilities.Append(std::move(paper_type_capability));
  printer.Set(kVendorCapability, std::move(vendor_capabilities));

  return printer;
}

base::DictValue* GetVendorCapabilityAtIndex(base::DictValue& printer,
                                            size_t index) {
  base::ListValue* vendor_capabilities_list =
      printer.FindList(kVendorCapability);
  if (!vendor_capabilities_list || index >= vendor_capabilities_list->size()) {
    return nullptr;
  }

  auto& ret = (*vendor_capabilities_list)[index];
  return ret.is_dict() ? &ret.GetDict() : nullptr;
}

base::ListValue ValidList(const base::ListValue* list) {
  base::ListValue out_list = list->Clone();
  out_list.EraseIf([](const base::Value& v) { return v.is_none(); });
  return out_list;
}

bool HasValidEntry(const base::ListValue* list) {
  return list && !list->empty() && !ValidList(list).empty();
}

void CompareStringKeys(const base::DictValue& expected,
                       const base::DictValue& actual,
                       std::string_view key) {
  EXPECT_EQ(*expected.FindString(key), *actual.FindString(key));
}

void ValidateList(const base::ListValue* list_out,
                  const base::ListValue* input_list) {
  base::ListValue input_list_valid = ValidList(input_list);
  ASSERT_EQ(list_out->size(), input_list_valid.size());
  for (size_t i = 0; i < list_out->size(); ++i) {
    EXPECT_EQ((*list_out)[i], input_list_valid[i]);
  }
}

void ValidateMedia(const base::DictValue* printer_out,
                   const base::ListValue* expected_list) {
  const base::ListValue* media_out = printer_out->FindList(kMediaSizes);
  if (!HasValidEntry(expected_list)) {
    EXPECT_FALSE(media_out);
    return;
  }
  ValidateList(media_out, expected_list);
}

void ValidateDpi(const base::DictValue* printer_out,
                 const base::DictValue* expected_dpi) {
  const base::DictValue* dpi_option_out = printer_out->FindDict(kDpi);
  if (!expected_dpi) {
    EXPECT_FALSE(dpi_option_out);
    return;
  }
  const base::ListValue* expected_dpi_list = expected_dpi->FindList(kOptionKey);
  if (!HasValidEntry(expected_dpi_list)) {
    EXPECT_FALSE(dpi_option_out);
    return;
  }
  ASSERT_TRUE(dpi_option_out);
  const base::ListValue* dpi_list_out = dpi_option_out->FindList(kOptionKey);
  ASSERT_TRUE(dpi_list_out);
  ValidateList(dpi_list_out, expected_dpi_list);
}

void ValidateCollate(const base::DictValue* printer_out) {
  std::optional<bool> collate_out = printer_out->FindBool(kCollate);
  ASSERT_TRUE(collate_out.has_value());
  EXPECT_TRUE(collate_out.value());
}

void ValidateVendorCaps(const base::DictValue* printer_out,
                        const base::ListValue* input_vendor_caps) {
  const base::ListValue* vendor_capability_out =
      printer_out->FindList(kVendorCapability);
  if (!HasValidEntry(input_vendor_caps)) {
    ASSERT_FALSE(vendor_capability_out);
    return;
  }

  ASSERT_TRUE(vendor_capability_out);
  ASSERT_EQ(vendor_capability_out->size(), input_vendor_caps->size());
  size_t index = 0;
  for (const auto& input_entry : *input_vendor_caps) {
    const auto& input_entry_dict = input_entry.GetDict();
    if (!HasValidEntry(
            input_entry_dict.FindDict(kSelectCapKey)->FindList(kOptionKey))) {
      continue;
    }
    const auto& current_vendor_capability_out =
        (*vendor_capability_out)[index].GetDict();
    CompareStringKeys(input_entry_dict, current_vendor_capability_out,
                      kDisplayName);
    CompareStringKeys(input_entry_dict, current_vendor_capability_out, kId);
    CompareStringKeys(input_entry_dict, current_vendor_capability_out,
                      kTypeKey);
    const base::DictValue* select_cap =
        current_vendor_capability_out.FindDict(kSelectCapKey);
    ASSERT_TRUE(select_cap);
    const base::ListValue* list = select_cap->FindList(kOptionKey);
    ASSERT_TRUE(list);
    ValidateList(
        list, input_entry_dict.FindDict(kSelectCapKey)->FindList(kOptionKey));
    index++;
  }
}

void ValidatePrinter(const base::DictValue& cdd_out,
                     const base::DictValue& printer) {
  const base::DictValue* printer_out = cdd_out.FindDict(kPrinter);
  ASSERT_TRUE(printer_out);

  const base::ListValue* media = printer.FindList(kMediaSizes);
  ValidateMedia(printer_out, media);

  const base::DictValue* dpi_dict = printer.FindDict(kDpi);
  ValidateDpi(printer_out, dpi_dict);
  ValidateCollate(printer_out);

  const base::ListValue* capabilities_list =
      printer.FindList(kVendorCapability);
  ValidateVendorCaps(printer_out, capabilities_list);
}

bool GetDpiResetToDefault(base::DictValue cdd) {
  const base::DictValue* printer = cdd.FindDict(kPrinter);
  const base::DictValue* dpi = printer->FindDict(kDpi);
  std::optional<bool> reset_to_default = dpi->FindBool(kResetToDefault);
  if (!reset_to_default.has_value()) {
    ADD_FAILURE();
    return false;
  }
  return reset_to_default.value();
}

// Returns a CDD with the media size options populated with `options`.
base::DictValue CreateCddWithMediaOptions(base::ListValue options) {
  base::DictValue media_size;
  media_size.Set(kOptionKey, std::move(options));
  base::DictValue printer;
  printer.Set(kMediaSizeKey, std::move(media_size));
  base::DictValue cdd;
  cdd.Set(kPrinter, std::move(printer));

  return cdd;
}

}  // namespace

using PrintPreviewUtilsTest = testing::Test;

TEST_F(PrintPreviewUtilsTest, FullCddPassthrough) {
  base::DictValue printer = GetCapabilitiesFull();
  base::DictValue cdd;
  cdd.Set(kPrinter, printer.Clone());
  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(cdd_out, printer);
}

TEST_F(PrintPreviewUtilsTest, FilterBadList) {
  // Set up the test expectations.
  base::DictValue printer = GetCapabilitiesFull();
  printer.Remove(kMediaSizes);

  // Clone the test expectations, and set bad media values.
  base::DictValue cdd;
  base::DictValue& cdd_printer = cdd.Set(kPrinter, printer.Clone())->GetDict();
  base::ListValue list_media;
  list_media.Append(base::Value());
  list_media.Append(base::Value());
  cdd_printer.Set(kMediaSizes, std::move(list_media));

  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(cdd_out, printer);
}

TEST_F(PrintPreviewUtilsTest, FilterBadOptionOneElement) {
  // Set up the test expectations.
  base::DictValue printer = GetCapabilitiesFull();
  printer.Remove(kDpi);
  base::DictValue dpi_600;
  dpi_600.Set(kHorizontalDpi, 600);
  dpi_600.Set(kVerticalDpi, 600);
  base::ListValue list_dpi;
  list_dpi.Append(std::move(dpi_600));
  base::DictValue options;
  options.Set(kOptionKey, std::move(list_dpi));
  printer.Set(kDpi, std::move(options));

  // Clone the test expectations, and insert a bad DPI value.
  base::DictValue cdd;
  base::DictValue& cdd_printer = cdd.Set(kPrinter, printer.Clone())->GetDict();
  base::DictValue* cdd_printer_dpi_dict = cdd_printer.FindDict(kDpi);
  ASSERT_TRUE(cdd_printer_dpi_dict);
  base::ListValue* cdd_printer_dpi_list =
      cdd_printer_dpi_dict->FindList(kOptionKey);
  ASSERT_TRUE(cdd_printer_dpi_list);
  cdd_printer_dpi_list->Insert(cdd_printer_dpi_list->begin(), base::Value());

  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(cdd_out, printer);
}

TEST_F(PrintPreviewUtilsTest, FilterBadOptionAllElement) {
  // Set up the test expectations.
  base::DictValue printer = GetCapabilitiesFull();
  printer.Remove(kDpi);

  // Clone the test expectations, and insert bad DPI values.
  base::DictValue cdd;
  base::DictValue& cdd_printer = cdd.Set(kPrinter, printer.Clone())->GetDict();
  base::ListValue list_dpi;
  list_dpi.Append(base::Value());
  list_dpi.Append(base::Value());
  base::DictValue options;
  options.Set(kOptionKey, std::move(list_dpi));
  cdd_printer.Set(kDpi, std::move(options));

  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(cdd_out, printer);
}

TEST_F(PrintPreviewUtilsTest, FilterBadVendorCapabilityAllElement) {
  // Start setting the test expectations.
  base::DictValue printer = GetCapabilitiesFull();

  // Clone the test expectations, and set bad vendor capabilities.
  base::DictValue cdd;
  base::DictValue& cdd_printer = cdd.Set(kPrinter, printer.Clone())->GetDict();
  base::DictValue* cdd_printer_cap_0 =
      GetVendorCapabilityAtIndex(cdd_printer, 0);
  ASSERT_TRUE(cdd_printer_cap_0);
  base::DictValue* select_cap_0 = cdd_printer_cap_0->FindDict(kSelectCapKey);
  ASSERT_TRUE(select_cap_0);
  base::ListValue option_list;
  option_list.Append(base::Value());
  option_list.Append(base::Value());
  select_cap_0->Set(kOptionKey, std::move(option_list));

  // Finish setting the test expectations, as the bad vendor capability should
  // be filtered out.
  base::ListValue* printer_vendor_capabilities_list =
      printer.FindList(kVendorCapability);
  ASSERT_TRUE(printer_vendor_capabilities_list);
  ASSERT_EQ(printer_vendor_capabilities_list->size(), 2u);
  printer_vendor_capabilities_list->erase(
      printer_vendor_capabilities_list->begin());

  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(cdd_out, printer);
}

TEST_F(PrintPreviewUtilsTest, FilterBadVendorCapabilityOneElement) {
  // Start setting the test expectations.
  base::DictValue printer = GetCapabilitiesFull();

  // Clone the test expectations, and set bad vendor capabilities.
  base::DictValue cdd;
  base::DictValue& cdd_printer = cdd.Set(kPrinter, printer.Clone())->GetDict();
  base::DictValue* cdd_printer_cap_0 =
      GetVendorCapabilityAtIndex(cdd_printer, 0);
  ASSERT_TRUE(cdd_printer_cap_0);
  base::DictValue* vendor_dict = cdd_printer_cap_0->FindDict(kSelectCapKey);
  ASSERT_TRUE(vendor_dict);
  base::ListValue pages_per_sheet;
  for (int i = 1; i <= 8; i *= 2) {
    if (i == 2) {
      pages_per_sheet.Append(base::Value());
      continue;
    }
    base::DictValue option;
    option.Set(kDisplayName, base::NumberToString(i));
    option.Set(kValue, i);
    if (i == 1) {
      option.Set(kIsDefault, true);
    }
    pages_per_sheet.Append(std::move(option));
  }
  vendor_dict->Set(kOptionKey, std::move(pages_per_sheet));

  // Finish setting the test expectations, as the bad vendor capability should
  // be filtered out.
  base::DictValue* printer_cap_0 = GetVendorCapabilityAtIndex(printer, 0);
  ASSERT_TRUE(printer_cap_0);
  base::DictValue* printer_vendor_dict = printer_cap_0->FindDict(kSelectCapKey);
  ASSERT_TRUE(printer_vendor_dict);
  base::ListValue* printer_vendor_list =
      printer_vendor_dict->FindList(kOptionKey);
  ASSERT_TRUE(printer_vendor_list);
  ASSERT_EQ(printer_vendor_list->size(), 4u);
  printer_vendor_list->erase(printer_vendor_list->begin() + 1);

  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(cdd_out, printer);
}

TEST_F(PrintPreviewUtilsTest, FilterBadDpis) {
  base::DictValue printer = GetCapabilitiesFull();

  base::DictValue cdd;
  base::DictValue& cdd_printer = cdd.Set(kPrinter, printer.Clone())->GetDict();
  base::DictValue* cdd_dpi_dict = cdd_printer.FindDict(kDpi);
  ASSERT_TRUE(cdd_dpi_dict);
  base::ListValue* cdd_dpi_list = cdd_dpi_dict->FindList(kOptionKey);
  ASSERT_TRUE(cdd_dpi_list);

  base::DictValue no_horizontal_dpi;
  no_horizontal_dpi.Set(kVerticalDpi, 150);
  cdd_dpi_list->Append(std::move(no_horizontal_dpi));

  base::DictValue no_vertical_dpi;
  no_vertical_dpi.Set(kVerticalDpi, 1200);
  cdd_dpi_list->Append(std::move(no_vertical_dpi));

  base::DictValue non_positive_horizontal_dpi;
  non_positive_horizontal_dpi.Set(kHorizontalDpi, -150);
  non_positive_horizontal_dpi.Set(kVerticalDpi, 150);
  cdd_dpi_list->Append(std::move(non_positive_horizontal_dpi));

  base::DictValue non_positive_vertical_dpi;
  non_positive_vertical_dpi.Set(kHorizontalDpi, 1200);
  non_positive_vertical_dpi.Set(kVerticalDpi, 0);
  cdd_dpi_list->Append(std::move(non_positive_vertical_dpi));

  cdd_dpi_list->Append("not a dict");

  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(cdd_out, printer);
}

TEST_F(PrintPreviewUtilsTest, CddResetToDefault) {
  base::DictValue printer = GetCapabilitiesFull();
  base::DictValue* dpi_dict = printer.FindDict(kDpi);

  base::DictValue cdd;
  dpi_dict->Set(kResetToDefault, true);
  cdd.Set(kPrinter, printer.Clone());
  auto cdd_out = ValidateCddForPrintPreview(cdd.Clone());
  ValidatePrinter(cdd_out, printer);
  EXPECT_TRUE(GetDpiResetToDefault(std::move(cdd_out)));

  dpi_dict->Set(kResetToDefault, false);
  cdd.Set(kPrinter, printer.Clone());
  cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(cdd_out, printer);
  EXPECT_FALSE(GetDpiResetToDefault(std::move(cdd_out)));
}

TEST_F(PrintPreviewUtilsTest, AddMissingDpi) {
  // Set up the test expectation to have only the 300 DPI setting, which is the
  // default DPI setting.
  base::DictValue printer = GetCapabilitiesFull();
  base::DictValue* dpi_dict = printer.FindDict(kDpi);
  ASSERT_TRUE(dpi_dict);
  base::ListValue* dpi_list = dpi_dict->FindList(kOptionKey);
  ASSERT_TRUE(dpi_list);
  ASSERT_EQ(2u, dpi_list->size());
  dpi_list->erase(dpi_list->begin() + 1);
  ASSERT_EQ(1u, dpi_list->size());
  ASSERT_EQ(
      base::test::ParseJson("{\"horizontal_dpi\": 300, \"vertical_dpi\": 300}"),
      (*dpi_list)[0]);

  // Initialize `cdd` but clear the DPI list.
  base::DictValue cdd;
  base::DictValue& cdd_printer = cdd.Set(kPrinter, printer.Clone())->GetDict();
  base::DictValue* cdd_printer_dpi_dict = cdd_printer.FindDict(kDpi);
  ASSERT_TRUE(cdd_printer_dpi_dict);
  base::ListValue* cdd_printer_dpi_list =
      cdd_printer_dpi_dict->FindList(kOptionKey);
  ASSERT_TRUE(cdd_printer_dpi_list);
  cdd_printer_dpi_list->clear();

  // ValidateCddForPrintPreview() should delete the `kDpi` key altogether, since
  // the associated value was an empty list.
  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  const base::DictValue* cdd_out_printer = cdd_out.FindDict(kPrinter);
  ASSERT_TRUE(cdd_out_printer);
  EXPECT_FALSE(cdd_out_printer->FindDict(kDpi));

  // Update `cdd_out` with the default value for this required capability. Then
  // `cdd_out` will pass validation.
  ValidatePrinter(UpdateCddWithDpiIfMissing(std::move(cdd_out)), printer);
}

TEST_F(PrintPreviewUtilsTest, ExistingValidDpiCapabilityDoesNotChange) {
  // Ensure the test expectation has multiple DPIs.
  const base::DictValue printer = GetCapabilitiesFull();
  const base::DictValue* dpi_dict = printer.FindDict(kDpi);
  ASSERT_TRUE(dpi_dict);
  const base::ListValue* dpi_list = dpi_dict->FindList(kOptionKey);
  ASSERT_TRUE(dpi_list);
  ASSERT_EQ(2u, dpi_list->size());

  // Initialize `cdd`, which is perfectly valid. It should pass through
  // ValidateCddForPrintPreview() and UpdateCddWithDpiIfMissing() without any
  // changes.
  base::DictValue cdd;
  cdd.Set(kPrinter, printer.Clone());
  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(UpdateCddWithDpiIfMissing(std::move(cdd_out)), printer);
}

TEST_F(PrintPreviewUtilsTest, FilterMediaSizesNoContinuousFeed) {
  base::DictValue media_1;
  media_1.Set(kMediaWidth, 100);
  media_1.Set(kMediaHeight, 200);
  base::DictValue media_2;
  media_2.Set(kMediaWidth, 300);
  media_2.Set(kMediaHeight, 400);
  base::ListValue option_list;
  option_list.Append(std::move(media_1));
  option_list.Append(std::move(media_2));

  base::ListValue expected_list = option_list.Clone();

  base::DictValue cdd = CreateCddWithMediaOptions(std::move(option_list));

  FilterContinuousFeedMediaSizes(cdd);

  const base::ListValue* options = GetMediaSizeOptionsFromCdd(cdd);
  ASSERT_TRUE(options);
  EXPECT_EQ(expected_list, *options);
}

TEST_F(PrintPreviewUtilsTest, FilterMediaSizesWithContinuousFeed) {
  base::DictValue media_1;
  media_1.Set(kMediaWidth, 100);
  media_1.Set(kMediaHeight, 200);
  base::DictValue media_2;
  media_2.Set(kMediaWidth, 300);
  media_2.Set(kMediaIsContinuousFeed, true);
  base::ListValue option_list;
  option_list.Append(media_1.Clone());
  option_list.Append(std::move(media_2));

  base::ListValue expected_list;
  expected_list.Append(std::move(media_1));

  base::DictValue cdd = CreateCddWithMediaOptions(std::move(option_list));

  FilterContinuousFeedMediaSizes(cdd);

  const base::ListValue* options = GetMediaSizeOptionsFromCdd(cdd);
  ASSERT_TRUE(options);
  EXPECT_EQ(expected_list, *options);
}

TEST_F(PrintPreviewUtilsTest, FilterMediaSizesAllContinuousFeed) {
  base::DictValue media_1;
  media_1.Set(kMediaWidth, 100);
  media_1.Set(kMediaIsContinuousFeed, true);
  base::DictValue media_2;
  media_2.Set(kMediaWidth, 300);
  media_2.Set(kMediaIsContinuousFeed, true);
  base::ListValue option_list;
  option_list.Append(std::move(media_1));
  option_list.Append(std::move(media_2));

  base::DictValue cdd = CreateCddWithMediaOptions(std::move(option_list));

  FilterContinuousFeedMediaSizes(cdd);

  const base::ListValue* options = GetMediaSizeOptionsFromCdd(cdd);
  ASSERT_TRUE(options);
  EXPECT_TRUE(options->empty());
}

}  // namespace printing
