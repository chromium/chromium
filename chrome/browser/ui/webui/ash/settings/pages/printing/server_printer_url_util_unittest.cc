// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/printing/server_printer_url_util.h"

#include <optional>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::settings {

class ServerPrinterUrlUtilTest : public testing::Test {
 public:
  ServerPrinterUrlUtilTest() = default;
  ~ServerPrinterUrlUtilTest() override = default;
};

TEST_F(ServerPrinterUrlUtilTest, IsValidScheme) {
  GURL gurl1("ipp://123.123.11.11:123");
  ASSERT_TRUE(HasValidServerPrinterScheme(gurl1));

  GURL gurl2("http://123.123.11.11:123");
  ASSERT_TRUE(HasValidServerPrinterScheme(gurl2));

  GURL gurl3("ipps://123.123.11.11:123");
  ASSERT_TRUE(HasValidServerPrinterScheme(gurl3));

  GURL gurl4("https://123.123.11.11:123");
  ASSERT_TRUE(HasValidServerPrinterScheme(gurl4));

  // Missing scheme.
  GURL gurl5("123.123.11.11:123");
  ASSERT_FALSE(HasValidServerPrinterScheme(gurl5));

  // Invalid scheme.
  GURL gurl6("test://123.123.11.11:123");
  ASSERT_FALSE(HasValidServerPrinterScheme(gurl6));
}

TEST_F(ServerPrinterUrlUtilTest, ConvertToGURL) {
  // Test that a GURL is created with |gurl1| as its source.
  std::string url1("http://123.123.11.11:631");
  std::optional<GURL> gurl1 = GenerateServerPrinterUrlWithValidScheme(url1);
  DCHECK(gurl1);
  ASSERT_EQ("http://123.123.11.11:631/", gurl1->spec());
  ASSERT_EQ("http", gurl1->scheme());
  ASSERT_EQ("631", gurl1->port());

  // Test that HTTPS is the default scheme if a scheme is not provided.
  std::string url2("123.123.11.11:631");
  std::optional<GURL> gurl2 = GenerateServerPrinterUrlWithValidScheme(url2);
  DCHECK(gurl2);
  ASSERT_EQ("https", gurl2->scheme());
  ASSERT_EQ("https://123.123.11.11:631/", gurl2->spec());

  // Test that if a URL has IPP as its scheme, it will create a new GURL with
  // HTTP as its scheme and 631 as its port.
  std::string url3("ipp://123.123.11.11");
  std::optional<GURL> gurl3 = GenerateServerPrinterUrlWithValidScheme(url3);
  DCHECK(gurl3);
  ASSERT_EQ("http", gurl3->scheme());
  ASSERT_EQ("631", gurl3->port());
  ASSERT_EQ("http://123.123.11.11:631/", gurl3->spec());

  // Test that if a URL has IPP as its scheme and a specified port, it will
  // create a new GURL with HTTP as the scheme and keeps the same port.
  std::string url4("ipp://123.123.11.11:321");
  std::optional<GURL> gurl4 = GenerateServerPrinterUrlWithValidScheme(url4);
  DCHECK(gurl4);
  ASSERT_EQ("http", gurl4->scheme());
  ASSERT_EQ("321", gurl4->port());
  ASSERT_EQ("http://123.123.11.11:321/", gurl4->spec());

  // Test that if a URL has IPPS as its scheme and a specified port, a new GURL
  // is created with the scheme as HTTPS and keeps the same port.
  std::string url5("ipps://123.123.11.11:555");
  std::optional<GURL> gurl5 = GenerateServerPrinterUrlWithValidScheme(url5);
  DCHECK(gurl5);
  ASSERT_EQ("https", gurl5->scheme());
  ASSERT_EQ("555", gurl5->port());
  ASSERT_EQ("https://123.123.11.11:555/", gurl5->spec());
}

}  // namespace ash::settings
