// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/public/cpp/cups_util.h"

#include <cups/ipp.h>

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "chromeos/printing/printer_configuration.h"
#include "printing/backend/cups_jobs.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cups_proxy {
namespace {

using chromeos::Printer;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pointwise;

// Matcher used to compare two Printers by display_name.
MATCHER(DisplayNameEq, "") {
  return std::get<0>(arg).display_name() == std::get<1>(arg).display_name();
}

const char kEndpointPrefix[] = "/printers/";

// Generated via base::GenerateGUID.
const char kDefaultPrinterId[] = "fd4c5f2e-7549-43d5-b931-9bf4e4f1bf51";

ipp_t* GetPrinterAttributesRequest(
    std::string printer_uri = kDefaultPrinterId) {
  ipp_t* ret = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);
  if (!ret) {
    return nullptr;
  }

  printer_uri = printing::PrinterUriFromName(printer_uri);
  ippAddString(ret, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", nullptr,
               printer_uri.c_str());
  return ret;
}

// Generates |num_printers| printers with unique display_names starting with
// |prefix|. Returned printers are in alphabetically ascending order.
std::vector<Printer> GenPrinters(int num_printers, base::StringPiece prefix) {
  std::vector<Printer> ret;
  for (int i = 0; i < num_printers; i++) {
    Printer printer;
    printer.set_display_name(base::StrCat({prefix, base::NumberToString(i)}));
    ret.push_back(printer);
  }

  // Alphabetically ascending order by display name.
  std::sort(ret.begin(), ret.end(), [](const Printer& a, const Printer& b) {
    return a.display_name() < b.display_name();
  });

  return ret;
}

std::vector<Printer> GetSavedPrinters(int num_printers) {
  return GenPrinters(num_printers, "SavedPrinter");
}

std::vector<Printer> GetEnterprisePrinters(int num_printers) {
  return GenPrinters(num_printers, "EnterprisePrinter");
}

TEST(GetPrinterIdTest, SimpleSanityTest) {
  auto printer_id = GetPrinterId(GetPrinterAttributesRequest());
  EXPECT_TRUE(printer_id);
  EXPECT_EQ(*printer_id, kDefaultPrinterId);
}

// PrinterId's must be non-empty.
TEST(GetPrinterIdTest, EmptyPrinterId) {
  EXPECT_FALSE(GetPrinterId(GetPrinterAttributesRequest("")));
}

TEST(GetPrinterIdTest, MissingPrinterUri) {
  ipp_t* ipp = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);
  EXPECT_FALSE(GetPrinterId(ipp));
}

// Embedded 'printer-uri' attribute must contain a '/'.
TEST(GetPrinterIdTest, MissingPathDelimiter) {
  ipp_t* ret = ippNewRequest(IPP_GET_PRINTER_ATTRIBUTES);
  if (!ret) {
    return;
  }

  // Omitting using printing::PrinterUriFromId to correctly embed the uri.
  ippAddString(ret, IPP_TAG_OPERATION, IPP_TAG_URI, "printer-uri", nullptr,
               kDefaultPrinterId);
  EXPECT_FALSE(GetPrinterId(ret));
}

TEST(ParseEndpointForPrinterIdTest, SimpleSanityTest) {
  base::Optional<std::string> printer_id = ParseEndpointForPrinterId(
      std::string(kEndpointPrefix) + kDefaultPrinterId);

  EXPECT_TRUE(printer_id.has_value());
  EXPECT_THAT(*printer_id, Not(IsEmpty()));
}

// PrinterId's must be non-empty.
TEST(ParseEndpointForPrinterIdTest, EmptyPrinterId) {
  EXPECT_FALSE(ParseEndpointForPrinterId(kEndpointPrefix));
}

// Endpoints must contain a '/'.
TEST(ParseEndpointForPrinterIdTest, MissingPathDelimiter) {
  EXPECT_FALSE(ParseEndpointForPrinterId(kDefaultPrinterId));
}

// If there are enough saved printers, we should just serve those.
TEST(FilterPrintersForPluginVmTest, EnoughSavedPrinters) {
  auto saved = GetSavedPrinters(kPluginVmPrinterLimit);
  auto enterprise = GetEnterprisePrinters(10);
  auto ret = FilterPrintersForPluginVm(saved, enterprise);
  EXPECT_THAT(saved, Pointwise(DisplayNameEq(), ret));
}

// Backfilled enterprise printers should be in alphabetical order, by
// display_name.
TEST(FilterPrintersForPluginVmTest, OrderedEnterprisePrinters) {
  auto enterprise = GetEnterprisePrinters(50);
  auto saved = GetSavedPrinters(10);

  // Filtered list should be saved printers + top enterprise printers up to the
  // limit.
  auto expected = saved;
  expected.insert(expected.end(), enterprise.begin(),
                  enterprise.begin() + (kPluginVmPrinterLimit - saved.size()));

  // We pre-shuffle the enterprise printers to test the ordering constraints.
  base::RandomShuffle(enterprise.begin(), enterprise.end());
  auto ret = FilterPrintersForPluginVm(saved, enterprise);
  EXPECT_THAT(ret, Pointwise(DisplayNameEq(), expected));

  // Test self-check.
  EXPECT_EQ(ret.front().display_name(), "SavedPrinter0");
  EXPECT_EQ(ret.back().display_name(), "EnterprisePrinter35");

  size_t last_saved_printer_idx = 9;
  EXPECT_EQ(ret[last_saved_printer_idx].display_name(), "SavedPrinter9");
  EXPECT_EQ(ret[last_saved_printer_idx + 1].display_name(),
            "EnterprisePrinter0");
}

}  // namespace
}  // namespace cups_proxy
