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

base::Value::Dict GetCapabilitiesFull() {
  base::Value::Dict printer;

  base::Value::List list_media;
  list_media.Append("Letter");
  list_media.Append("A4");
  printer.Set(kMediaSizes, std::move(list_media));

  base::Value::Dict dpi_300;
  dpi_300.Set(kHorizontalDpi, 300);
  dpi_300.Set(kVerticalDpi, 300);
  base::Value::Dict dpi_600;
  dpi_600.Set(kHorizontalDpi, 600);
  dpi_600.Set(kVerticalDpi, 600);
  base::Value::List list_dpi;
  list_dpi.Append(std::move(dpi_300));
  list_dpi.Append(std::move(dpi_600));

  base::Value::Dict options;
  options.Set(kOptionKey, std::move(list_dpi));
  printer.Set(kDpi, std::move(options));

  printer.Set(kCollate, true);

  base::Value::List pages_per_sheet;
  for (int i = 1; i <= 8; i *= 2) {
    base::Value::Dict option;
    option.Set(kDisplayName, base::NumberToString(i));
    option.Set(kValue, i);
    if (i == 1)
      option.Set(kIsDefault, true);
    pages_per_sheet.Append(std::move(option));
  }
  base::Value::Dict pages_per_sheet_option;
  pages_per_sheet_option.Set(kOptionKey, std::move(pages_per_sheet));
  base::Value::Dict pages_per_sheet_capability;
  pages_per_sheet_capability.Set(kDisplayName, kPagesPerSheet);
  pages_per_sheet_capability.Set(kId, kPagesPerSheet);
  pages_per_sheet_capability.Set(kTypeKey, kSelectString);
  pages_per_sheet_capability.Set(kSelectCapKey,
                                 std::move(pages_per_sheet_option));

  base::Value::List paper_types;
  base::Value::Dict option1;
  option1.Set(kDisplayName, "Plain");
  option1.Set(kValue, "Plain");
  option1.Set(kIsDefault, true);
  base::Value::Dict option2;
  option2.Set(kDisplayName, "Photo");
  option2.Set(kValue, "Photo");
  paper_types.Append(std::move(option1));
  paper_types.Append(std::move(option2));
  base::Value::Dict paper_type_option;
  paper_type_option.Set(kOptionKey, std::move(paper_types));
  base::Value::Dict paper_type_capability;
  paper_type_capability.Set(kDisplayName, kPaperType);
  paper_type_capability.Set(kId, kPaperType);
  paper_type_capability.Set(kTypeKey, kSelectString);
  paper_type_capability.Set(kSelectCapKey, std::move(paper_type_option));

  base::Value::List vendor_capabilities;
  vendor_capabilities.Append(std::move(pages_per_sheet_capability));
  vendor_capabilities.Append(std::move(paper_type_capability));
  printer.Set(kVendorCapability, std::move(vendor_capabilities));

  return printer;
}

base::Value::Dict* GetVendorCapabilityAtIndex(base::Value::Dict& printer,
                                              size_t index) {
  base::Value::List* vendor_capabilities_list =
      printer.FindList(kVendorCapability);
  if (!vendor_capabilities_list || index >= vendor_capabilities_list->size())
    return nullptr;

  auto& ret = (*vendor_capabilities_list)[index];
  return ret.is_dict() ? &ret.GetDict() : nullptr;
}

base::Value::List ValidList(const base::Value::List* list) {
  base::Value::List out_list = list->Clone();
  out_list.EraseIf([](const base::Value& v) { return v.is_none(); });
  return out_list;
}

bool HasValidEntry(const base::Value::List* list) {
  return list && !list->empty() && !ValidList(list).empty();
}

void CompareStringKeys(const base::Value::Dict& expected,
                       const base::Value::Dict& actual,
                       std::string_view key) {
  EXPECT_EQ(*expected.FindString(key), *actual.FindString(key));
}

void ValidateList(const base::Value::List* list_out,
                  const base::Value::List* input_list) {
  base::Value::List input_list_valid = ValidList(input_list);
  ASSERT_EQ(list_out->size(), input_list_valid.size());
  for (size_t i = 0; i < list_out->size(); ++i) {
    EXPECT_EQ((*list_out)[i], input_list_valid[i]);
  }
}

void ValidateMedia(const base::Value::Dict* printer_out,
                   const base::Value::List* expected_list) {
  const base::Value::List* media_out = printer_out->FindList(kMediaSizes);
  if (!HasValidEntry(expected_list)) {
    EXPECT_FALSE(media_out);
    return;
  }
  ValidateList(media_out, expected_list);
}

void ValidateDpi(const base::Value::Dict* printer_out,
                 const base::Value::Dict* expected_dpi) {
  const base::Value::Dict* dpi_option_out = printer_out->FindDict(kDpi);
  if (!expected_dpi) {
    EXPECT_FALSE(dpi_option_out);
    return;
  }
  const base::Value::List* expected_dpi_list =
      expected_dpi->FindList(kOptionKey);
  if (!HasValidEntry(expected_dpi_list)) {
    EXPECT_FALSE(dpi_option_out);
    return;
  }
  ASSERT_TRUE(dpi_option_out);
  const base::Value::List* dpi_list_out = dpi_option_out->FindList(kOptionKey);
  ASSERT_TRUE(dpi_list_out);
  ValidateList(dpi_list_out, expected_dpi_list);
}

void ValidateCollate(const base::Value::Dict* printer_out) {
  std::optional<bool> collate_out = printer_out->FindBool(kCollate);
  ASSERT_TRUE(collate_out.has_value());
  EXPECT_TRUE(collate_out.value());
}

void ValidateVendorCaps(const base::Value::Dict* printer_out,
                        const base::Value::List* input_vendor_caps) {
  const base::Value::List* vendor_capability_out =
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
    const base::Value::Dict* select_cap =
        current_vendor_capability_out.FindDict(kSelectCapKey);
    ASSERT_TRUE(select_cap);
    const base::Value::List* list = select_cap->FindList(kOptionKey);
    ASSERT_TRUE(list);
    ValidateList(
        list, input_entry_dict.FindDict(kSelectCapKey)->FindList(kOptionKey));
    index++;
  }
}

void ValidatePrinter(const base::Value::Dict& cdd_out,
                     const base::Value::Dict& printer) {
  const base::Value::Dict* printer_out = cdd_out.FindDict(kPrinter);
  ASSERT_TRUE(printer_out);

  const base::Value::List* media = printer.FindList(kMediaSizes);
  ValidateMedia(printer_out, media);

  const base::Value::Dict* dpi_dict = printer.FindDict(kDpi);
  ValidateDpi(printer_out, dpi_dict);
  ValidateCollate(printer_out);

  const base::Value::List* capabilities_list =
      printer.FindList(kVendorCapability);
  ValidateVendorCaps(printer_out, capabilities_list);
}

bool GetDpiResetToDefault(base::Value::Dict cdd) {
  const base::Value::Dict* printer = cdd.FindDict(kPrinter);
  const base::Value::Dict* dpi = printer->FindDict(kDpi);
  std::optional<bool> reset_to_default = dpi->FindBool(kResetToDefault);
  if (!reset_to_default.has_value()) {
    ADD_FAILURE();
    return false;
  }
  return reset_to_default.value();
}

// Returns a CDD with the media size options populated with `options`.
base::Value::Dict CreateCddWithMediaOptions(base::Value::List options) {
  base::Value::Dict media_size;
  media_size.Set(kOptionKey, std::move(options));
  base::Value::Dict printer;
  printer.Set(kMediaSizeKey, std::move(media_size));
  base::Value::Dict cdd;
  cdd.Set(kPrinter, std::move(printer));

  return cdd;
}

}  // namespace

using PrintPreviewUtilsTest = testing::Test;

TEST_F(PrintPreviewUtilsTest, FullCddPassthrough) {
  base::Value::Dict printer = GetCapabilitiesFull();
  base::Value::Dict cdd;
  cdd.Set(kPrinter, printer.Clone());
  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(cdd_out, printer);
}

TEST_F(PrintPreviewUtilsTest, FilterBadList) {
  // Set up the test expectations.
  base::Value::Dict printer = GetCapabilitiesFull();
  printer.Remove(kMediaSizes);

  // Clone the test expectations, and set bad media values.
  base::Value::Dict cdd;
  base::Value::Dict& cdd_printer =
      cdd.Set(kPrinter, printer.Clone())->GetDict();
  base::Value::List list_media;
  list_media.Append(base::Value());
  list_media.Append(base::Value());
  cdd_printer.Set(kMediaSizes, std::move(list_media));

  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(cdd_out, printer);
}

TEST_F(PrintPreviewUtilsTest, FilterBadOptionOneElement) {
  // Set up the test expectations.
  base::Value::Dict printer = GetCapabilitiesFull();
  printer.Remove(kDpi);
  base::Value::Dict dpi_600;
  dpi_600.Set(kHorizontalDpi, 600);
  dpi_600.Set(kVerticalDpi, 600);
  base::Value::List list_dpi;
  list_dpi.Append(std::move(dpi_600));
  base::Value::Dict options;
  options.Set(kOptionKey, std::move(list_dpi));
  printer.Set(kDpi, std::move(options));

  // Clone the test expectations, and insert a bad DPI value.
  base::Value::Dict cdd;
  base::Value::Dict& cdd_printer =
      cdd.Set(kPrinter, printer.Clone())->GetDict();
  base::Value::Dict* cdd_printer_dpi_dict = cdd_printer.FindDict(kDpi);
  ASSERT_TRUE(cdd_printer_dpi_dict);
  base::Value::List* cdd_printer_dpi_list =
      cdd_printer_dpi_dict->FindList(kOptionKey);
  ASSERT_TRUE(cdd_printer_dpi_list);
  cdd_printer_dpi_list->Insert(cdd_printer_dpi_list->begin(), base::Value());

  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(cdd_out, printer);
}

TEST_F(PrintPreviewUtilsTest, FilterBadOptionAllElement) {
  // Set up the test expectations.
  base::Value::Dict printer = GetCapabilitiesFull();
  printer.Remove(kDpi);

  // Clone the test expectations, and insert bad DPI values.
  base::Value::Dict cdd;
  base::Value::Dict& cdd_printer =
      cdd.Set(kPrinter, printer.Clone())->GetDict();
  base::Value::List list_dpi;
  list_dpi.Append(base::Value());
  list_dpi.Append(base::Value());
  base::Value::Dict options;
  options.Set(kOptionKey, std::move(list_dpi));
  cdd_printer.Set(kDpi, std::move(options));

  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(cdd_out, printer);
}

TEST_F(PrintPreviewUtilsTest, FilterBadVendorCapabilityAllElement) {
  // Start setting the test expectations.
  base::Value::Dict printer = GetCapabilitiesFull();

  // Clone the test expectations, and set bad vendor capabilities.
  base::Value::Dict cdd;
  base::Value::Dict& cdd_printer =
      cdd.Set(kPrinter, printer.Clone())->GetDict();
  base::Value::Dict* cdd_printer_cap_0 =
      GetVendorCapabilityAtIndex(cdd_printer, 0);
  ASSERT_TRUE(cdd_printer_cap_0);
  base::Value::Dict* select_cap_0 = cdd_printer_cap_0->FindDict(kSelectCapKey);
  ASSERT_TRUE(select_cap_0);
  base::Value::List option_list;
  option_list.Append(base::Value());
  option_list.Append(base::Value());
  select_cap_0->Set(kOptionKey, std::move(option_list));

  // Finish setting the test expectations, as the bad vendor capability should
  // be filtered out.
  base::Value::List* printer_vendor_capabilities_list =
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
  base::Value::Dict printer = GetCapabilitiesFull();

  // Clone the test expectations, and set bad vendor capabilities.
  base::Value::Dict cdd;
  base::Value::Dict& cdd_printer =
      cdd.Set(kPrinter, printer.Clone())->GetDict();
  base::Value::Dict* cdd_printer_cap_0 =
      GetVendorCapabilityAtIndex(cdd_printer, 0);
  ASSERT_TRUE(cdd_printer_cap_0);
  base::Value::Dict* vendor_dict = cdd_printer_cap_0->FindDict(kSelectCapKey);
  ASSERT_TRUE(vendor_dict);
  base::Value::List pages_per_sheet;
  for (int i = 1; i <= 8; i *= 2) {
    if (i == 2) {
      pages_per_sheet.Append(base::Value());
      continue;
    }
    base::Value::Dict option;
    option.Set(kDisplayName, base::NumberToString(i));
    option.Set(kValue, i);
    if (i == 1)
      option.Set(kIsDefault, true);
    pages_per_sheet.Append(std::move(option));
  }
  vendor_dict->Set(kOptionKey, std::move(pages_per_sheet));

  // Finish setting the test expectations, as the bad vendor capability should
  // be filtered out.
  base::Value::Dict* printer_cap_0 = GetVendorCapabilityAtIndex(printer, 0);
  ASSERT_TRUE(printer_cap_0);
  base::Value::Dict* printer_vendor_dict =
      printer_cap_0->FindDict(kSelectCapKey);
  ASSERT_TRUE(printer_vendor_dict);
  base::Value::List* printer_vendor_list =
      printer_vendor_dict->FindList(kOptionKey);
  ASSERT_TRUE(printer_vendor_list);
  ASSERT_EQ(printer_vendor_list->size(), 4u);
  printer_vendor_list->erase(printer_vendor_list->begin() + 1);

  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(cdd_out, printer);
}

TEST_F(PrintPreviewUtilsTest, FilterBadDpis) {
  base::Value::Dict printer = GetCapabilitiesFull();

  base::Value::Dict cdd;
  base::Value::Dict& cdd_printer =
      cdd.Set(kPrinter, printer.Clone())->GetDict();
  base::Value::Dict* cdd_dpi_dict = cdd_printer.FindDict(kDpi);
  ASSERT_TRUE(cdd_dpi_dict);
  base::Value::List* cdd_dpi_list = cdd_dpi_dict->FindList(kOptionKey);
  ASSERT_TRUE(cdd_dpi_list);

  base::Value::Dict no_horizontal_dpi;
  no_horizontal_dpi.Set(kVerticalDpi, 150);
  cdd_dpi_list->Append(std::move(no_horizontal_dpi));

  base::Value::Dict no_vertical_dpi;
  no_vertical_dpi.Set(kVerticalDpi, 1200);
  cdd_dpi_list->Append(std::move(no_vertical_dpi));

  base::Value::Dict non_positive_horizontal_dpi;
  non_positive_horizontal_dpi.Set(kHorizontalDpi, -150);
  non_positive_horizontal_dpi.Set(kVerticalDpi, 150);
  cdd_dpi_list->Append(std::move(non_positive_horizontal_dpi));

  base::Value::Dict non_positive_vertical_dpi;
  non_positive_vertical_dpi.Set(kHorizontalDpi, 1200);
  non_positive_vertical_dpi.Set(kVerticalDpi, 0);
  cdd_dpi_list->Append(std::move(non_positive_vertical_dpi));

  cdd_dpi_list->Append("not a dict");

  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(cdd_out, printer);
}

TEST_F(PrintPreviewUtilsTest, CddResetToDefault) {
  base::Value::Dict printer = GetCapabilitiesFull();
  base::Value::Dict* dpi_dict = printer.FindDict(kDpi);

  base::Value::Dict cdd;
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
  base::Value::Dict printer = GetCapabilitiesFull();
  base::Value::Dict* dpi_dict = printer.FindDict(kDpi);
  ASSERT_TRUE(dpi_dict);
  base::Value::List* dpi_list = dpi_dict->FindList(kOptionKey);
  ASSERT_TRUE(dpi_list);
  ASSERT_EQ(2u, dpi_list->size());
  dpi_list->erase(dpi_list->begin() + 1);
  ASSERT_EQ(1u, dpi_list->size());
  ASSERT_EQ(
      base::test::ParseJson("{\"horizontal_dpi\": 300, \"vertical_dpi\": 300}"),
      (*dpi_list)[0]);

  // Initialize `cdd` but clear the DPI list.
  base::Value::Dict cdd;
  base::Value::Dict& cdd_printer =
      cdd.Set(kPrinter, printer.Clone())->GetDict();
  base::Value::Dict* cdd_printer_dpi_dict = cdd_printer.FindDict(kDpi);
  ASSERT_TRUE(cdd_printer_dpi_dict);
  base::Value::List* cdd_printer_dpi_list =
      cdd_printer_dpi_dict->FindList(kOptionKey);
  ASSERT_TRUE(cdd_printer_dpi_list);
  cdd_printer_dpi_list->clear();

  // ValidateCddForPrintPreview() should delete the `kDpi` key altogether, since
  // the associated value was an empty list.
  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  const base::Value::Dict* cdd_out_printer = cdd_out.FindDict(kPrinter);
  ASSERT_TRUE(cdd_out_printer);
  EXPECT_FALSE(cdd_out_printer->FindDict(kDpi));

  // Update `cdd_out` with the default value for this required capability. Then
  // `cdd_out` will pass validation.
  ValidatePrinter(UpdateCddWithDpiIfMissing(std::move(cdd_out)), printer);
}

TEST_F(PrintPreviewUtilsTest, ExistingValidDpiCapabilityDoesNotChange) {
  // Ensure the test expectation has multiple DPIs.
  const base::Value::Dict printer = GetCapabilitiesFull();
  const base::Value::Dict* dpi_dict = printer.FindDict(kDpi);
  ASSERT_TRUE(dpi_dict);
  const base::Value::List* dpi_list = dpi_dict->FindList(kOptionKey);
  ASSERT_TRUE(dpi_list);
  ASSERT_EQ(2u, dpi_list->size());

  // Initialize `cdd`, which is perfectly valid. It should pass through
  // ValidateCddForPrintPreview() and UpdateCddWithDpiIfMissing() without any
  // changes.
  base::Value::Dict cdd;
  cdd.Set(kPrinter, printer.Clone());
  auto cdd_out = ValidateCddForPrintPreview(std::move(cdd));
  ValidatePrinter(UpdateCddWithDpiIfMissing(std::move(cdd_out)), printer);
}

TEST_F(PrintPreviewUtilsTest, FilterMediaSizesNoContinuousFeed) {
  base::Value::Dict media_1;
  media_1.Set(kMediaWidth, 100);
  media_1.Set(kMediaHeight, 200);
  base::Value::Dict media_2;
  media_2.Set(kMediaWidth, 300);
  media_2.Set(kMediaHeight, 400);
  base::Value::List option_list;
  option_list.Append(std::move(media_1));
  option_list.Append(std::move(media_2));

  base::Value::List expected_list = option_list.Clone();

  base::Value::Dict cdd = CreateCddWithMediaOptions(std::move(option_list));

  FilterContinuousFeedMediaSizes(cdd);

  const base::Value::List* options = GetMediaSizeOptionsFromCdd(cdd);
  ASSERT_TRUE(options);
  EXPECT_EQ(expected_list, *options);
}

TEST_F(PrintPreviewUtilsTest, FilterMediaSizesWithContinuousFeed) {
  base::Value::Dict media_1;
  media_1.Set(kMediaWidth, 100);
  media_1.Set(kMediaHeight, 200);
  base::Value::Dict media_2;
  media_2.Set(kMediaWidth, 300);
  media_2.Set(kMediaIsContinuousFeed, true);
  base::Value::List option_list;
  option_list.Append(media_1.Clone());
  option_list.Append(std::move(media_2));

  base::Value::List expected_list;
  expected_list.Append(std::move(media_1));

  base::Value::Dict cdd = CreateCddWithMediaOptions(std::move(option_list));

  FilterContinuousFeedMediaSizes(cdd);

  const base::Value::List* options = GetMediaSizeOptionsFromCdd(cdd);
  ASSERT_TRUE(options);
  EXPECT_EQ(expected_list, *options);
}

TEST_F(PrintPreviewUtilsTest, FilterMediaSizesAllContinuousFeed) {
  base::Value::Dict media_1;
  media_1.Set(kMediaWidth, 100);
  media_1.Set(kMediaIsContinuousFeed, true);
  base::Value::Dict media_2;
  media_2.Set(kMediaWidth, 300);
  media_2.Set(kMediaIsContinuousFeed, true);
  base::Value::List option_list;
  option_list.Append(std::move(media_1));
  option_list.Append(std::move(media_2));

  base::Value::Dict cdd = CreateCddWithMediaOptions(std::move(option_list));

  FilterContinuousFeedMediaSizes(cdd);

  const base::Value::List* options = GetMediaSizeOptionsFromCdd(cdd);
  ASSERT_TRUE(options);
  EXPECT_TRUE(options->empty());
}

}  // namespace printing
