// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/printing/usb_printer_id.h"

#include <map>
#include <string>

#include "base/ranges/algorithm.h"
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
  base::ranges::copy(device_id_str, std::back_inserter(ret));
  return ret;
}

TEST(UsbPrinterIdTest, EmptyDeviceId) {
  EXPECT_THAT(BuildDeviceIdMapping({}), IsEmpty());
}

// Tests that we get the same map back after parsing.
TEST(UsbPrinterIdTest, SimpleSanityTest) {
  MapType mapping = GetDefaultDeviceId();
  std::vector<uint8_t> buffer = MapToBuffer(mapping);

  // Output also includes original buffer without the two leading size bytes.
  MapType expected = mapping;
  expected["CHROMEOS_RAW_ID"].emplace_back(
      reinterpret_cast<const char*>(buffer.data()) + 2, buffer.size() - 2);

  EXPECT_EQ(expected, BuildDeviceIdMapping(buffer));
}

}  // namespace
}  // namespace chromeos
