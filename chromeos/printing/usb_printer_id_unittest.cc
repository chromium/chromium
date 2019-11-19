// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/usb_printer_id.h"

#include <algorithm>
#include <map>
#include <string>

#include "base/strings/string_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

using testing::IsEmpty;

using MapType = std::map<std::string, std::vector<std::string>>;

MapType GetDefaultDeviceId() {
  MapType ret;

  // Make.
  ret["MFG"].push_back("EPSON");

  // Model.
  ret["MDL"].push_back("ET-2700");

  // Command set.
  ret["CMD"].push_back("ESCPL2");
  ret["CMD"].push_back("BDC");
  ret["CMD"].push_back("D4");
  ret["CMD"].push_back("END4");
  ret["CMD"].push_back("GENEP");

  return ret;
}

std::string MapToString(const MapType& map) {
  std::vector<std::string> terms;
  for (auto& term : map) {
    std::string values = base::JoinString(term.second, ",");
    terms.push_back(base::JoinString({term.first, values}, ":"));
  }

  std::string device_id_str = "xx";  // Two unused bytes for the length.
  device_id_str += base::JoinString(terms, ";") + ";";
  return device_id_str;
}

std::vector<uint8_t> MapToBuffer(const MapType& map) {
  std::string device_id_str = MapToString(map);

  std::vector<uint8_t> ret;
  std::copy(device_id_str.begin(), device_id_str.end(),
            std::back_inserter(ret));
  return ret;
}

TEST(UsbPrinterIdTest, EmptyDeviceId) {
  EXPECT_THAT(BuildDeviceIdMapping({}), IsEmpty());
}

// Tests that we get the same map back after parsing.
TEST(UsbPrinterIdTest, SimpleSanityTest) {
  MapType mapping = GetDefaultDeviceId();
  std::vector<uint8_t> buffer = MapToBuffer(mapping);
  EXPECT_EQ(mapping, BuildDeviceIdMapping(buffer));
}

}  // namespace
}  // namespace chromeos
