// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/epson_driver_matching.h"

#include <string>
#include <vector>

#include "chromeos/printing/ppd_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

const char kEscPr[] = "ESCPR1";

using PrinterDiscoveryType = PrinterSearchData::PrinterDiscoveryType;

// Returns PrinterSearchData that will match the Epson Generic PPD.
PrinterSearchData GetTestPrinterSearchData() {
  PrinterSearchData sd;
  sd.discovery_type = PrinterDiscoveryType::kUsb;
  sd.make_and_model.push_back("epson");
  sd.printer_id.set_command_set({kEscPr});
  return sd;
}

// Ensuring simple good cases generated above pass.
TEST(EpsonDriverMatchingTest, SimpleSanityTest) {
  EXPECT_TRUE(CanUseEpsonGenericPPD(GetTestPrinterSearchData()));
}

// Always fails printers missing make and model information.
TEST(EpsonDriverMatchingTest, EmptyMakeAndModels) {
  PrinterSearchData sd(GetTestPrinterSearchData());
  sd.make_and_model.clear();
  EXPECT_FALSE(CanUseEpsonGenericPPD(sd));
}

// Always fails printers with invalid discovery types.
TEST(EpsonDriverMatchingTest, InvalidPrinterDiscoveryType) {
  PrinterSearchData sd(GetTestPrinterSearchData());
  sd.discovery_type = PrinterDiscoveryType::kUnknown;
  EXPECT_FALSE(CanUseEpsonGenericPPD(sd));

  sd.discovery_type = PrinterDiscoveryType::kDiscoveryTypeMax;
  EXPECT_FALSE(CanUseEpsonGenericPPD(sd));
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

// Simple PrinterDiscoveryType::kUsb checks.
TEST(EpsonDriverMatchingTest, UsbDiscovery) {
  PrinterSearchData sd(GetTestPrinterSearchData());
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

}  // namespace
}  // namespace chromeos
