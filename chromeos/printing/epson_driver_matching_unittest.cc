// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/epson_driver_matching.h"

#include <string>
#include <vector>

#include "chromeos/printing/ppd_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

const char kOctetStream[] = "application/octet-stream";
const char kEscPr[] = "ESCPR1";
const char kEpsonEscpr[] = "application/vnd.epson.escpr";

using PrinterDiscoveryType = PrinterSearchData::PrinterDiscoveryType;

PrinterSearchData GetTestPrinterSearchData(
    PrinterDiscoveryType type = PrinterDiscoveryType::kManual) {
  PrinterSearchData sd;
  sd.make_and_model.push_back("epson");

  switch (type) {
    case PrinterDiscoveryType::kManual:
      sd.discovery_type = PrinterDiscoveryType::kManual;
      sd.supported_document_formats.push_back(kOctetStream);
      break;

    case PrinterDiscoveryType::kUsb:
      sd.discovery_type = PrinterDiscoveryType::kUsb;
      sd.printer_id.set_command_set({kEscPr});
      break;

    case PrinterDiscoveryType::kZeroconf:
      sd.discovery_type = PrinterDiscoveryType::kZeroconf;
      sd.supported_document_formats.push_back(kEpsonEscpr);
      break;

    default:
      sd.discovery_type = type;
      break;
  }

  return sd;
}

// Ensuring simple good cases generated above pass.
TEST(EpsonDriverMatchingTest, SimpleSanityTest) {
  EXPECT_TRUE(CanUseEpsonGenericPPD(
      GetTestPrinterSearchData(PrinterDiscoveryType::kManual)));
  EXPECT_TRUE(CanUseEpsonGenericPPD(
      GetTestPrinterSearchData(PrinterDiscoveryType::kUsb)));
  EXPECT_TRUE(CanUseEpsonGenericPPD(
      GetTestPrinterSearchData(PrinterDiscoveryType::kZeroconf)));
}

// Always fails printers missing make and model information.
TEST(EpsonDriverMatchingTest, EmptyMakeAndModels) {
  EXPECT_FALSE(CanUseEpsonGenericPPD(PrinterSearchData()));
}

// Always fails printers with invalid discovery types.
TEST(EpsonDriverMatchingTest, InvalidPrinterDiscoveryType) {
  EXPECT_FALSE(CanUseEpsonGenericPPD(
      GetTestPrinterSearchData(PrinterDiscoveryType::kUnknown)));
  EXPECT_FALSE(CanUseEpsonGenericPPD(
      GetTestPrinterSearchData(PrinterDiscoveryType::kDiscoveryTypeMax)));
}

// Confirms an Epson printer if any make and models have 'epson'.
TEST(EpsonDriverMatchingTest, ChecksAllMakeAndModels) {
  PrinterSearchData sd(GetTestPrinterSearchData());
  sd.make_and_model.clear();

  sd.make_and_model.push_back("kodak fdasf");
  EXPECT_FALSE(CanUseEpsonGenericPPD(sd));

  sd.make_and_model.push_back("epso nomega x301");
  EXPECT_FALSE(CanUseEpsonGenericPPD(sd));

  sd.make_and_model.push_back("epson xp5100");
  EXPECT_TRUE(CanUseEpsonGenericPPD(sd));
}

// Simple PrinterDiscoveryType::kManual checks.
TEST(EpsonDriverMatchingTest, ManualDiscovery) {
  PrinterSearchData sd(GetTestPrinterSearchData(PrinterDiscoveryType::kManual));
  sd.supported_document_formats.clear();

  sd.supported_document_formats.push_back("application/");
  EXPECT_FALSE(CanUseEpsonGenericPPD(sd));

  sd.supported_document_formats.push_back(std::string(kOctetStream) + "afds");
  EXPECT_FALSE(CanUseEpsonGenericPPD(sd));

  sd.printer_id.set_command_set({kOctetStream});
  EXPECT_FALSE(CanUseEpsonGenericPPD(sd));

  sd.supported_document_formats.push_back(kOctetStream);
  EXPECT_TRUE(CanUseEpsonGenericPPD(sd));
}

// Simple PrinterDiscoveryType::kUsb checks.
TEST(EpsonDriverMatchingTest, UsbDiscovery) {
  PrinterSearchData sd(GetTestPrinterSearchData(PrinterDiscoveryType::kUsb));
  std::vector<std::string> command_set;

  command_set.push_back("ESC");
  sd.printer_id.set_command_set(command_set);
  EXPECT_FALSE(CanUseEpsonGenericPPD(sd));

  command_set.push_back("abcd" + std::string(kEscPr));
  sd.printer_id.set_command_set(command_set);
  EXPECT_FALSE(CanUseEpsonGenericPPD(sd));

  sd.supported_document_formats.push_back(kEscPr);
  EXPECT_FALSE(CanUseEpsonGenericPPD(sd));

  command_set.push_back(std::string(kEscPr) + "garbage");
  sd.printer_id.set_command_set(command_set);
  EXPECT_TRUE(CanUseEpsonGenericPPD(sd));
}

TEST(EpsonDriverMatchingTest, ZerconfDiscovery) {
  PrinterSearchData sd(
      GetTestPrinterSearchData(PrinterDiscoveryType::kZeroconf));
  sd.supported_document_formats.clear();

  sd.supported_document_formats.push_back("application/");
  EXPECT_FALSE(CanUseEpsonGenericPPD(sd));

  sd.supported_document_formats.push_back(std::string(kEpsonEscpr) + ":asfd");
  EXPECT_FALSE(CanUseEpsonGenericPPD(sd));

  sd.printer_id.set_command_set({kEpsonEscpr});
  EXPECT_FALSE(CanUseEpsonGenericPPD(sd));

  sd.supported_document_formats.push_back(kEpsonEscpr);
  EXPECT_TRUE(CanUseEpsonGenericPPD(sd));
}

}  // namespace
}  // namespace chromeos
