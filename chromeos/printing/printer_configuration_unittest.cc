// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/printer_configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(PrinterConfigurationTest, EmptyScheme) {
  chromeos::Printer printer;
  EXPECT_FALSE(printer.SetUri("://hostname.com/"));
}

TEST(PrinterConfigurationTest, JustScheme) {
  chromeos::Printer printer;
  EXPECT_FALSE(printer.SetUri("ipps://"));
}

TEST(PrinterConfigurationTest, InvalidPort) {
  chromeos::Printer printer;
  EXPECT_FALSE(printer.SetUri("ipp://1.2.3.4:abcd"));
}

TEST(PrinterConfigurationTest, MissingScheme) {
  chromeos::Printer printer;
  EXPECT_FALSE(printer.SetUri("/10.2.12.3/"));
}

TEST(PrinterConfigurationTest, ParseUriIp) {
  chromeos::Printer printer;
  EXPECT_TRUE(printer.SetUri("ipp://192.168.1.5"));
  EXPECT_EQ(printer.uri().GetScheme(), "ipp");
  EXPECT_EQ(printer.uri().GetHost(), "192.168.1.5");
  EXPECT_TRUE(printer.uri().GetPathEncodedAsString().empty());
}

TEST(PrinterConfigurationTest, ParseUriPort) {
  chromeos::Printer printer;
  EXPECT_TRUE(printer.SetUri("ipp://1.2.3.4:4444"));
  EXPECT_EQ(printer.uri().GetPort(), 4444);
}

TEST(PrinterConfigurationTest, ParseTrailingSlash) {
  chromeos::Printer printer;
  EXPECT_TRUE(printer.SetUri("ipp://1.2.3.4/"));
  EXPECT_EQ(printer.uri().GetPathEncodedAsString(), "");
  EXPECT_EQ(printer.uri().GetHost(), "1.2.3.4");
}

TEST(PrinterConfigurationTest, ParseUriHostNameAndPort) {
  chromeos::Printer printer;
  EXPECT_TRUE(printer.SetUri("ipp://chromium.org:8"));
  EXPECT_EQ(printer.uri().GetPort(), 8);
  EXPECT_EQ(printer.uri().GetHost(), "chromium.org");
}

TEST(PrinterConfigurationTest, ParseUriPathNoPort) {
  chromeos::Printer printer;
  EXPECT_TRUE(printer.SetUri("ipps://chromium.org/printers/printprint"));
  EXPECT_EQ(printer.uri().GetHost(), "chromium.org");
  EXPECT_EQ(printer.uri().GetPathEncodedAsString(), "/printers/printprint");
}

TEST(PrinterConfigurationTest, ParseUriSubdomainQueueAndPort) {
  chromeos::Printer printer;
  EXPECT_TRUE(printer.SetUri("ipp://codesearch.chromium.org:1234/ipp/print"));
  EXPECT_EQ(printer.uri().GetHost(), "codesearch.chromium.org");
  EXPECT_EQ(printer.uri().GetPort(), 1234);
  EXPECT_EQ(printer.uri().GetPathEncodedAsString(), "/ipp/print");
}

TEST(PrinterConfigurationTest, SecureProtocolIpps) {
  chromeos::Printer printer;
  EXPECT_TRUE(printer.SetUri("ipps://1.2.3.4"));
  EXPECT_TRUE(printer.HasSecureProtocol());
}

TEST(PrinterConfigurationTest, SecureProtocolHttps) {
  chromeos::Printer printer;
  EXPECT_TRUE(printer.SetUri("https://1.2.3.4"));
  EXPECT_TRUE(printer.HasSecureProtocol());
}

TEST(PrinterConfigurationTest, SecureProtocolUsb) {
  chromeos::Printer printer;
  EXPECT_TRUE(printer.SetUri("usb://host/path"));
  EXPECT_TRUE(printer.HasSecureProtocol());
}

TEST(PrinterConfigurationTest, SecureProtocolIppusb) {
  chromeos::Printer printer;
  EXPECT_TRUE(printer.SetUri("ippusb://host/path"));
  EXPECT_TRUE(printer.HasSecureProtocol());
}

TEST(PrinterConfigurationTest, NonSecureProtocolIpp) {
  chromeos::Printer printer;
  EXPECT_TRUE(printer.SetUri("ipp://1.2.3.4"));
  EXPECT_FALSE(printer.HasSecureProtocol());
}

TEST(PrinterConfigurationTest, NonSecureProtocolHttp) {
  chromeos::Printer printer;
  EXPECT_TRUE(printer.SetUri("http://1.2.3.4"));
  EXPECT_FALSE(printer.HasSecureProtocol());
}

TEST(PrinterConfigurationTest, NonSecureProtocolSocket) {
  chromeos::Printer printer;
  EXPECT_TRUE(printer.SetUri("socket://1.2.3.4"));
  EXPECT_FALSE(printer.HasSecureProtocol());
}

TEST(PrinterConfigurationTest, NonSecureProtocolLpd) {
  chromeos::Printer printer;
  EXPECT_TRUE(printer.SetUri("lpd://1.2.3.4"));
  EXPECT_FALSE(printer.HasSecureProtocol());
}

TEST(PrinterConfigurationTest, NonSecureProtocolUnknown) {
  chromeos::Printer printer;
  EXPECT_FALSE(printer.SetUri("foobar"));
  EXPECT_FALSE(printer.HasSecureProtocol());
}

TEST(PrinterConfigurationTest, DriverlessUsbForced) {
  chromeos::Printer printer;
  printer.set_make_and_model("Epson WF-110 Series");
  EXPECT_TRUE(printer.SetUri("usb://1234/5678"));
  EXPECT_TRUE(printer.RequiresDriverlessUsb());
}

TEST(PrinterConfigurationTest, DriverlessUsbNotForced) {
  chromeos::Printer printer;
  printer.set_make_and_model("Epson XP-7100 Series");
  EXPECT_TRUE(printer.SetUri("usb://1234/5678"));
  EXPECT_FALSE(printer.RequiresDriverlessUsb());
}

}  // namespace
