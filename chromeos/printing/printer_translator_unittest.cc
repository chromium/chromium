// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/printer_translator.h"

#include <optional>
#include <string>

#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chromeos/printing/cups_printer_status.h"
#include "chromeos/printing/printer_configuration.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

using CupsPrinterStatusReason = CupsPrinterStatus::CupsPrinterStatusReason;

namespace {

// Printer test data
const char kHash[] = "ABCDEF123456";
const char kName[] = "Chrome Super Printer";
const char kDescription[] = "first star on the left";
const char kUri[] = "ipp://printy.domain.co:555/ipp/print";
const char kUsbUri[] = "usb://1234/af9d?serial=ink1";
const char kUUID[] = "UUID-UUID-UUID";

const char kMake[] = "Chrome";
const char kModel[] = "Inktastic Laser Magic";
const char kMakeAndModel[] = "Chrome Inktastic Laser Magic";

const char kGUID[] = "{4d8faf22-303f-46c6-ab30-352d47d6a8b9}";

// PpdReference test data
const char kEffectiveMakeAndModel[] = "PrintBlaster LazerInker 2000";

Printer CreateGenericPrinter() {
  Printer ret;
  ret.set_id(kHash);
  ret.set_display_name(kName);
  ret.set_description(kDescription);
  ret.set_make_and_model(kMakeAndModel);
  CupsPrinterStatus cups_printer_status(kHash);
  cups_printer_status.AddStatusReason(
      CupsPrinterStatusReason::Reason::kDoorOpen,
      CupsPrinterStatusReason::Severity::kError);
  ret.set_printer_status(cups_printer_status);
  return ret;
}

Printer CreateAutoconfPrinter() {
  Printer ret = CreateGenericPrinter();
  ret.mutable_ppd_reference()->autoconf = true;
  return ret;
}

// Check the values populated in |printer_info| match the values of |printer|.
void CheckGenericPrinterInfo(const Printer& printer,
                             const base::Value::Dict& printer_info) {
  ExpectDictStringValue(printer.id(), printer_info, "printerId");
  ExpectDictStringValue(printer.display_name(), printer_info, "printerName");
  ExpectDictStringValue(printer.description(), printer_info,
                        "printerDescription");
  ExpectDictStringValue(printer.make_and_model(), printer_info,
                        "printerMakeAndModel");

  base::Value::Dict printer_status_dict =
      printer.printer_status().ConvertToValue();
  const std::string* printer_id = printer_status_dict.FindString("printerId");
  ASSERT_TRUE(printer_id);
  EXPECT_EQ(printer.printer_status().GetPrinterId(), *printer_id);

  base::Value::List* status_reasons =
      printer_status_dict.FindList("statusReasons");
  ASSERT_TRUE(status_reasons);
  ASSERT_EQ(1u, status_reasons->size());
  base::Value::Dict& status_reason_dict = (*status_reasons)[0].GetDict();
  EXPECT_THAT(status_reason_dict.FindInt("reason"),
              testing::Optional(static_cast<int>(
                  CupsPrinterStatusReason::Reason::kDoorOpen)));
  EXPECT_THAT(status_reason_dict.FindInt("severity"),
              testing::Optional(
                  static_cast<int>(CupsPrinterStatusReason::Severity::kError)));
}

// Check that the corresponding values in |printer_info| match the given URI
// components of |address|, |queue|, and |protocol|.
void CheckPrinterInfoUri(const base::Value::Dict& printer_info,
                         const std::string& protocol,
                         const std::string& address,
                         const std::string& queue) {
  ExpectDictStringValue(address, printer_info, "printerAddress");
  ExpectDictStringValue(queue, printer_info, "printerQueue");
  ExpectDictStringValue(protocol, printer_info, "printerProtocol");
}

}  // anonymous namespace

TEST(PrinterTranslatorTest, RecommendedPrinterToPrinterMissingId) {
  base::Value::Dict value;
  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(value);

  EXPECT_FALSE(printer);
}

TEST(PrinterTranslatorTest, MissingDisplayNameFails) {
  base::Value::Dict preference;
  preference.Set("id", kHash);
  // display name omitted
  preference.Set("uri", kUri);
  preference.SetByDottedPath("ppd_resource.effective_model",
                             kEffectiveMakeAndModel);

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  EXPECT_FALSE(printer);
}

TEST(PrinterTranslatorTest, MissingUriFails) {
  base::Value::Dict preference;
  preference.Set("id", kHash);
  preference.Set("display_name", kName);
  // uri omitted
  preference.SetByDottedPath("ppd_resource.effective_model",
                             kEffectiveMakeAndModel);

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  EXPECT_FALSE(printer);
}

TEST(PrinterTranslatorTest, MissingPpdResourceFails) {
  base::Value::Dict preference;
  preference.Set("id", kHash);
  preference.Set("display_name", kName);
  preference.Set("uri", kUri);
  // ppd resource omitted

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  EXPECT_FALSE(printer);
}

TEST(PrinterTranslatorTest, MissingEffectiveMakeModelFails) {
  base::Value::Dict preference;
  preference.Set("id", kHash);
  preference.Set("display_name", kName);
  preference.Set("uri", kUri);
  preference.SetByDottedPath("ppd_resource.foobarwrongfield", "gibberish");

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  EXPECT_FALSE(printer);
}

// The test verifies that setting both true autoconf flag and non-empty
// effective_model properties is not considered as the valid policy.
TEST(PrinterTranslatorTest, AutoconfAndMakeModelSet) {
  base::Value::Dict preference;
  preference.Set("id", kHash);
  preference.Set("display_name", kName);
  preference.Set("uri", kUri);
  preference.SetByDottedPath("ppd_resource.effective_model",
                             kEffectiveMakeAndModel);
  preference.SetByDottedPath("ppd_resource.autoconf", true);

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  EXPECT_FALSE(printer);
}

TEST(PrinterTranslatorTest, InvalidUriFails) {
  base::Value::Dict preference;
  preference.Set("id", kHash);
  preference.Set("display_name", kName);
  preference.SetByDottedPath("ppd_resource.effective_model",
                             kEffectiveMakeAndModel);

  // uri with incorrect port
  preference.Set("uri", "ipp://hostname.tld:-1");

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  EXPECT_FALSE(printer);
}

TEST(PrinterTranslatorTest, RecommendedPrinterMinimalSetup) {
  base::Value::Dict preference;
  preference.Set("id", kHash);
  preference.Set("display_name", kName);
  preference.Set("uri", kUri);
  preference.SetByDottedPath("ppd_resource.effective_model",
                             kEffectiveMakeAndModel);

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  ASSERT_TRUE(printer);

  EXPECT_EQ(kEffectiveMakeAndModel,
            printer->ppd_reference().effective_make_and_model);
  EXPECT_EQ(false, printer->ppd_reference().autoconf);
}

TEST(PrinterTranslatorTest, RecommendedPrinterToPrinter) {
  base::Value::Dict preference;
  preference.Set("id", kHash);
  preference.Set("display_name", kName);
  preference.Set("description", kDescription);
  preference.Set("manufacturer", kMake);
  preference.Set("model", kModel);
  preference.Set("uri", kUri);
  preference.Set("uuid", kUUID);

  preference.SetByDottedPath("ppd_resource.autoconf", false);
  preference.SetByDottedPath("ppd_resource.effective_model",
                             kEffectiveMakeAndModel);

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  EXPECT_TRUE(printer);

  EXPECT_EQ(kHash, printer->id());
  EXPECT_EQ(kName, printer->display_name());
  EXPECT_EQ(kDescription, printer->description());
  EXPECT_EQ(kMakeAndModel, printer->make_and_model());
  EXPECT_EQ(kUri, printer->uri().GetNormalized());
  EXPECT_EQ(kUUID, printer->uuid());

  EXPECT_EQ(kEffectiveMakeAndModel,
            printer->ppd_reference().effective_make_and_model);
  EXPECT_EQ(false, printer->ppd_reference().autoconf);
}

TEST(PrinterTranslatorTest, RecommendedPrinterToPrinterAutoconf) {
  base::Value::Dict preference;
  preference.Set("id", kHash);
  preference.Set("display_name", kName);
  preference.Set("uri", kUri);

  preference.SetByDottedPath("ppd_resource.autoconf", true);

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  EXPECT_TRUE(printer);

  EXPECT_EQ(kHash, printer->id());
  EXPECT_EQ(kName, printer->display_name());
  EXPECT_EQ(kUri, printer->uri().GetNormalized());

  EXPECT_EQ(true, printer->ppd_reference().autoconf);
}

TEST(PrinterTranslatorTest, RecommendedPrinterToPrinterBlankManufacturer) {
  base::Value::Dict preference;
  preference.Set("id", kHash);
  preference.Set("display_name", kName);
  preference.Set("model", kModel);
  preference.Set("uri", kUri);
  preference.SetByDottedPath("ppd_resource.effective_model",
                             kEffectiveMakeAndModel);

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  EXPECT_TRUE(printer);

  EXPECT_EQ(kModel, printer->make_and_model());
}

TEST(PrinterTranslatorTest, RecommendedPrinterToPrinterBlankModel) {
  base::Value::Dict preference;
  preference.Set("id", kHash);
  preference.Set("display_name", kName);
  preference.Set("manufacturer", kMake);
  preference.Set("uri", kUri);
  preference.SetByDottedPath("ppd_resource.effective_model",
                             kEffectiveMakeAndModel);

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  EXPECT_TRUE(printer);

  EXPECT_EQ(kMake, printer->make_and_model());
}

TEST(PrinterTranslatorTest, BulkPrinterJson) {
  base::Value::Dict preference;
  preference.Set("guid", kGUID);
  preference.Set("display_name", kName);
  preference.Set("uri", kUri);
  preference.SetByDottedPath("ppd_resource.effective_model",
                             kEffectiveMakeAndModel);

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  EXPECT_TRUE(printer);

  EXPECT_EQ(kGUID, printer->id());
}

TEST(PrinterTranslatorTest, GetCupsPrinterInfoGenericPrinter) {
  Printer printer = CreateGenericPrinter();
  base::Value::Dict printer_info = GetCupsPrinterInfo(printer);
  CheckGenericPrinterInfo(CreateGenericPrinter(), printer_info);

  // We expect the default values to be set for the URI components since the
  // generic printer does not have the URI field set.
  CheckPrinterInfoUri(printer_info, "ipp", "", "");

  ExpectDictBooleanValue(false, printer_info, "printerPpdReference.autoconf");
}

TEST(PrinterTranslatorTest, GetCupsPrinterInfoGenericPrinterWithUri) {
  Printer printer = CreateGenericPrinter();
  ASSERT_TRUE(printer.SetUri(kUri));

  base::Value::Dict printer_info = GetCupsPrinterInfo(printer);
  CheckGenericPrinterInfo(CreateGenericPrinter(), printer_info);

  CheckPrinterInfoUri(printer_info, "ipp", "printy.domain.co:555", "ipp/print");

  ExpectDictBooleanValue(false, printer_info, "printerPpdReference.autoconf");
}

TEST(PrinterTranslatorTest, GetCupsPrinterInfoGenericPrinterWithUsbUri) {
  Printer printer = CreateGenericPrinter();
  ASSERT_TRUE(printer.SetUri(kUsbUri));

  base::Value::Dict printer_info = GetCupsPrinterInfo(printer);
  CheckGenericPrinterInfo(CreateGenericPrinter(), printer_info);

  CheckPrinterInfoUri(printer_info, "usb", "1234", "af9d?serial=ink1");

  ExpectDictBooleanValue(false, printer_info, "printerPpdReference.autoconf");
}

TEST(PrinterTranslatorTest, GetCupsPrinterInfoAutoconfPrinter) {
  Printer printer = CreateAutoconfPrinter();
  base::Value::Dict printer_info = GetCupsPrinterInfo(printer);
  CheckGenericPrinterInfo(CreateGenericPrinter(), printer_info);

  // We expect the default values to be set for the URI components since the
  // generic printer does not have the URI field set.
  CheckPrinterInfoUri(printer_info, "ipp", "", "");

  // Since this is an autoconf printer we expect "printerPpdReference.autoconf"
  // to be true.
  ExpectDictBooleanValue(true, printer_info, "printerPpdReference.autoconf");
}

TEST(PrinterTranslatorTest, GetCupsPrinterInfoManagedPrinter) {
  Printer printer = CreateGenericPrinter();
  printer.set_source(Printer::Source::SRC_USER_PREFS);
  base::Value::Dict printer_info = GetCupsPrinterInfo(printer);
  ExpectDictBooleanValue(false, printer_info, "isManaged");

  printer.set_source(Printer::Source::SRC_POLICY);
  printer_info = GetCupsPrinterInfo(printer);
  ExpectDictBooleanValue(true, printer_info, "isManaged");
}

}  // namespace chromeos
