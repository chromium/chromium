// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/public/cpp/cups_util.h"

#include <cups/ipp.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
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
// |prefix|.
std::vector<Printer> GenPrinters(int num_printers, std::string_view prefix) {
  std::vector<Printer> ret;
  for (int i = 0; i < num_printers; i++) {
    Printer printer;
    printer.set_display_name(base::StrCat({prefix, base::NumberToString(i)}));
    ret.push_back(printer);
  }
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
  std::optional<std::string> printer_id = ParseEndpointForPrinterId(
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
  auto ret = FilterPrintersForPluginVm(saved, enterprise, {});
  EXPECT_THAT(ret, Pointwise(DisplayNameEq(), saved));
}

// If there are less than kPluginVmPrinterLimit printers, serve all of them.
TEST(FilterPrintersForPluginVmTest, VeryFewSavedPrinters) {
  auto saved = GetSavedPrinters(4);
  auto enterprise = GetEnterprisePrinters(1);
  std::vector<std::string> recent = {saved[2].id(), saved[1].id(),
                                     saved[3].id()};
  std::vector<Printer> expected = {saved[2], saved[1], saved[3], saved[0],
                                   enterprise[0]};
  auto ret = FilterPrintersForPluginVm(saved, enterprise, recent);
  EXPECT_THAT(ret, Pointwise(DisplayNameEq(), expected));
}

// Make sure stale recent printers (printers not found in either the saved or
// enterprise list) are ignored.
TEST(FilterPrintersForPluginVmTest, StaleSavedPrinters) {
  auto stale = GetSavedPrinters(3);
  auto enterprise = GetEnterprisePrinters(2);
  std::vector<std::string> recent = {stale[0].id(), stale[1].id(),
                                     enterprise[1].id(), stale[2].id()};
  std::vector<Printer> expected = {enterprise[1], enterprise[0]};
  auto ret = FilterPrintersForPluginVm({}, enterprise, recent);
  EXPECT_THAT(ret, Pointwise(DisplayNameEq(), expected));
}

// Serve printers in the following order: recent printers followed by saved
// printers followed by enterprise printers, up to the limit.
TEST(FilterPrintersForPluginVmTest, EnterprisePrinters) {
  auto saved = GetSavedPrinters(10);
  auto enterprise = GetEnterprisePrinters(50);
  std::vector<std::string> recent = {enterprise[1].id(), saved[1].id()};

  std::vector<Printer> expected = {enterprise[1], saved[1], saved[0]};
  expected.insert(expected.end(), saved.begin() + 2, saved.end());
  expected.push_back(enterprise[0]);
  expected.insert(expected.end(), enterprise.begin() + 2, enterprise.end());
  if (expected.size() > kPluginVmPrinterLimit)
    expected.resize(kPluginVmPrinterLimit);

  auto ret = FilterPrintersForPluginVm(saved, enterprise, recent);
  EXPECT_THAT(ret, Pointwise(DisplayNameEq(), expected));

  // Test self-check.
  EXPECT_EQ(ret.front().display_name(), "EnterprisePrinter1");
  EXPECT_EQ(ret[1].display_name(), "SavedPrinter1");
  EXPECT_EQ(ret[2].display_name(), "SavedPrinter0");
  EXPECT_EQ(ret[3].display_name(), "SavedPrinter2");
  EXPECT_EQ(ret.back().display_name(), "EnterprisePrinter29");

  size_t last_saved_printer_idx = 10;
  EXPECT_EQ(ret[last_saved_printer_idx].display_name(), "SavedPrinter9");
  EXPECT_EQ(ret[last_saved_printer_idx + 1].display_name(),
            "EnterprisePrinter0");
  EXPECT_EQ(ret[last_saved_printer_idx + 2].display_name(),
            "EnterprisePrinter2");
}

}  // namespace
}  // namespace cups_proxy
