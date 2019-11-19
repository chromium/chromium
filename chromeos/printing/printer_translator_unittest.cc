// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/printing/printer_configuration.h"
#include "chromeos/printing/printer_translator.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

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
  ret.set_manufacturer(kMake);
  ret.set_model(kModel);
  ret.set_make_and_model(kMakeAndModel);
  return ret;
}

Printer CreateAutoconfPrinter() {
  Printer ret = CreateGenericPrinter();
  ret.mutable_ppd_reference()->autoconf = true;
  return ret;
}

// Check the values populated in |printer_info| match the values of |printer|.
void CheckGenericPrinterInfo(const Printer& printer,
                             const base::DictionaryValue& printer_info) {
  ExpectDictStringValue(printer.id(), printer_info, "printerId");
  ExpectDictStringValue(printer.display_name(), printer_info, "printerName");
  ExpectDictStringValue(printer.description(), printer_info,
                        "printerDescription");
  ExpectDictStringValue(printer.manufacturer(), printer_info,
                        "printerManufacturer");
  ExpectDictStringValue(printer.model(), printer_info, "printerModel");
  ExpectDictStringValue(printer.make_and_model(), printer_info,
                        "printerMakeAndModel");
}

// Check that the corresponding values in |printer_info| match the given URI
// components of |address|, |queue|, and |protocol|.
void CheckPrinterInfoUri(const base::DictionaryValue& printer_info,
                         const std::string& protocol,
                         const std::string& address,
                         const std::string& queue) {
  ExpectDictStringValue(address, printer_info, "printerAddress");
  ExpectDictStringValue(queue, printer_info, "printerQueue");
  ExpectDictStringValue(protocol, printer_info, "printerProtocol");
}

}  // anonymous namespace

TEST(PrinterTranslatorTest, RecommendedPrinterToPrinterMissingId) {
  base::DictionaryValue value;
  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(value);

  EXPECT_FALSE(printer);
}

TEST(PrinterTranslatorTest, MissingDisplayNameFails) {
  base::DictionaryValue preference;
  preference.SetString("id", kHash);
  // display name omitted
  preference.SetString("uri", kUri);
  preference.SetString("ppd_resource.effective_model", kEffectiveMakeAndModel);

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  EXPECT_FALSE(printer);
}

TEST(PrinterTranslatorTest, MissingUriFails) {
  base::DictionaryValue preference;
  preference.SetString("id", kHash);
  preference.SetString("display_name", kName);
  // uri omitted
  preference.SetString("ppd_resource.effective_model", kEffectiveMakeAndModel);

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  EXPECT_FALSE(printer);
}

TEST(PrinterTranslatorTest, MissingPpdResourceFails) {
  base::DictionaryValue preference;
  preference.SetString("id", kHash);
  preference.SetString("display_name", kName);
  preference.SetString("uri", kUri);
  // ppd resource omitted

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  EXPECT_FALSE(printer);
}

TEST(PrinterTranslatorTest, MissingEffectiveMakeModelFails) {
  base::DictionaryValue preference;
  preference.SetString("id", kHash);
  preference.SetString("display_name", kName);
  preference.SetString("uri", kUri);
  preference.SetString("ppd_resource.foobarwrongfield", "gibberish");

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  EXPECT_FALSE(printer);
}

// The test verifies that setting both true autoconf flag and non-empty
// effective_model properties is not considered as the valid policy.
TEST(PrinterTranslatorTest, AutoconfAndMakeModelSet) {
  base::DictionaryValue preference;
  preference.SetString("id", kHash);
  preference.SetString("display_name", kName);
  preference.SetString("uri", kUri);
  preference.SetString("ppd_resource.effective_model", kEffectiveMakeAndModel);
  preference.SetBoolean("ppd_resource.autoconf", true);

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  EXPECT_FALSE(printer);
}

TEST(PrinterTranslatorTest, InvalidUriFails) {
  base::DictionaryValue preference;
  preference.SetString("id", kHash);
  preference.SetString("display_name", kName);
  preference.SetString("ppd_resource.effective_model", kEffectiveMakeAndModel);

  // uri with dangling colon
  preference.SetString("uri", "ipp://hostname.tld:");

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  EXPECT_FALSE(printer);
}

TEST(PrinterTranslatorTest, RecommendedPrinterMinimalSetup) {
  base::DictionaryValue preference;
  preference.SetString("id", kHash);
  preference.SetString("display_name", kName);
  preference.SetString("uri", kUri);
  preference.SetString("ppd_resource.effective_model", kEffectiveMakeAndModel);

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  ASSERT_TRUE(printer);

  EXPECT_EQ(kEffectiveMakeAndModel,
            printer->ppd_reference().effective_make_and_model);
  EXPECT_EQ(false, printer->ppd_reference().autoconf);
}

TEST(PrinterTranslatorTest, RecommendedPrinterToPrinter) {
  base::DictionaryValue preference;
  preference.SetString("id", kHash);
  preference.SetString("display_name", kName);
  preference.SetString("description", kDescription);
  preference.SetString("manufacturer", kMake);
  preference.SetString("model", kModel);
  preference.SetString("uri", kUri);
  preference.SetString("uuid", kUUID);

  preference.SetBoolean("ppd_resource.autoconf", false);
  preference.SetString("ppd_resource.effective_model", kEffectiveMakeAndModel);

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  EXPECT_TRUE(printer);

  EXPECT_EQ(kHash, printer->id());
  EXPECT_EQ(kName, printer->display_name());
  EXPECT_EQ(kDescription, printer->description());
  EXPECT_EQ(kMake, printer->manufacturer());
  EXPECT_EQ(kModel, printer->model());
  EXPECT_EQ(kMakeAndModel, printer->make_and_model());
  EXPECT_EQ(kUri, printer->uri());
  EXPECT_EQ(kUUID, printer->uuid());

  EXPECT_EQ(kEffectiveMakeAndModel,
            printer->ppd_reference().effective_make_and_model);
  EXPECT_EQ(false, printer->ppd_reference().autoconf);
}

TEST(PrinterTranslatorTest, RecommendedPrinterToPrinterAutoconf) {
  base::DictionaryValue preference;
  preference.SetString("id", kHash);
  preference.SetString("display_name", kName);
  preference.SetString("uri", kUri);

  preference.SetBoolean("ppd_resource.autoconf", true);

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  EXPECT_TRUE(printer);

  EXPECT_EQ(kHash, printer->id());
  EXPECT_EQ(kName, printer->display_name());
  EXPECT_EQ(kUri, printer->uri());

  EXPECT_EQ(true, printer->ppd_reference().autoconf);
}

TEST(PrinterTranslatorTest, RecommendedPrinterToPrinterBlankManufacturer) {
  base::DictionaryValue preference;
  preference.SetString("id", kHash);
  preference.SetString("display_name", kName);
  preference.SetString("model", kModel);
  preference.SetString("uri", kUri);
  preference.SetString("ppd_resource.effective_model", kEffectiveMakeAndModel);

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  EXPECT_TRUE(printer);

  EXPECT_EQ(kModel, printer->model());
  EXPECT_EQ(kModel, printer->make_and_model());
}

TEST(PrinterTranslatorTest, RecommendedPrinterToPrinterBlankModel) {
  base::DictionaryValue preference;
  preference.SetString("id", kHash);
  preference.SetString("display_name", kName);
  preference.SetString("manufacturer", kMake);
  preference.SetString("uri", kUri);
  preference.SetString("ppd_resource.effective_model", kEffectiveMakeAndModel);

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  EXPECT_TRUE(printer);

  EXPECT_EQ(kMake, printer->manufacturer());
  EXPECT_EQ(kMake, printer->make_and_model());
}

TEST(PrinterTranslatorTest, BulkPrinterJson) {
  base::DictionaryValue preference;
  preference.SetString("guid", kGUID);
  preference.SetString("display_name", kName);
  preference.SetString("uri", kUri);
  preference.SetString("ppd_resource.effective_model", kEffectiveMakeAndModel);

  std::unique_ptr<Printer> printer = RecommendedPrinterToPrinter(preference);
  EXPECT_TRUE(printer);

  EXPECT_EQ(kGUID, printer->id());
}

TEST(PrinterTranslatorTest, GetCupsPrinterInfoGenericPrinter) {
  Printer printer = CreateGenericPrinter();
  std::unique_ptr<base::DictionaryValue> printer_info =
      GetCupsPrinterInfo(printer);
  CheckGenericPrinterInfo(CreateGenericPrinter(), *printer_info);

  // We expect the default values to be set for the URI components since the
  // generic printer does not have the URI field set.
  CheckPrinterInfoUri(*printer_info, "ipp", "", "");

  ExpectDictBooleanValue(false, *printer_info, "printerPpdReference.autoconf");
}

TEST(PrinterTranslatorTest, GetCupsPrinterInfoGenericPrinterWithUri) {
  Printer printer = CreateGenericPrinter();
  printer.set_uri(kUri);

  std::unique_ptr<base::DictionaryValue> printer_info =
      GetCupsPrinterInfo(printer);
  CheckGenericPrinterInfo(CreateGenericPrinter(), *printer_info);

  CheckPrinterInfoUri(*printer_info, "ipp", "printy.domain.co:555",
                      "ipp/print");

  ExpectDictBooleanValue(false, *printer_info, "printerPpdReference.autoconf");
}

TEST(PrinterTranslatorTest, GetCupsPrinterInfoGenericPrinterWithUsbUri) {
  Printer printer = CreateGenericPrinter();
  printer.set_uri(kUsbUri);

  std::unique_ptr<base::DictionaryValue> printer_info =
      GetCupsPrinterInfo(printer);
  CheckGenericPrinterInfo(CreateGenericPrinter(), *printer_info);

  CheckPrinterInfoUri(*printer_info, "usb", "1234/af9d?serial=ink1", "");

  ExpectDictBooleanValue(false, *printer_info, "printerPpdReference.autoconf");
}

TEST(PrinterTranslatorTest, GetCupsPrinterInfoAutoconfPrinter) {
  Printer printer = CreateAutoconfPrinter();
  std::unique_ptr<base::DictionaryValue> printer_info =
      GetCupsPrinterInfo(printer);
  CheckGenericPrinterInfo(CreateGenericPrinter(), *printer_info);

  // We expect the default values to be set for the URI components since the
  // generic printer does not have the URI field set.
  CheckPrinterInfoUri(*printer_info, "ipp", "", "");

  // Since this is an autoconf printer we expect "printerPpdReference.autoconf"
  // to be true.
  ExpectDictBooleanValue(true, *printer_info, "printerPpdReference.autoconf");
}

}  // namespace chromeos
