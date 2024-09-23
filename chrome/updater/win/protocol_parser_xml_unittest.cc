// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/protocol_parser_xml.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(ProtocolParserXML, Success) {
  const char kUpdateResponse[] =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<response protocol=\"3.0\">"
      "  <systemrequirements platform=\"win\" arch=\"x64\" "
      "      min_os_version=\"6.1\"/>"
      "  <app appid=\"{8A69D345-D564-463C-AFF1-A69D9E530F96}\" status=\"ok\">"
      "    <updatecheck status=\"ok\">"
      "      <urls>"
      "        <url codebase=\"http://dl.google.com/edgedl/chrome/install/\"/>"
      "        <url codebase=\"http://dl.google.com/fallback/install/\"/>"
      "      </urls>"
      "      <manifest version=\"1.2.3.4\">"
      "        <packages>"
      "          <package name=\"installer.exe\" hash_sha256=\"SamPLE_SHA==\" "
      "              size=\"112233\"/>"
      "        </packages>"
      "        <actions>"
      "          <action event=\"install\" run=\"installer.exe\" "
      "              arguments=\"--do-not-launch-chrome\"/>"
      "          <action event=\"postinstall\" "
      "              onsuccess=\"exitsilentlyonlaunchcmd\"/>"
      "        </actions>"
      "      </manifest>"
      "    </updatecheck>"
      "    <data index=\"verboselogging\" name=\"install\" status=\"ok\">"
      "      {"
      "        \"distribution\": {"
      "          \"verbose_logging\": true"
      "        }"
      "      }"
      "    </data>"
      "    <data index=\"defaultbrowser\" name=\"install\" status=\"ok\">"
      "      {"
      "        \"distribution\": {"
      "          \"make_chrome_default_for_user\": true"
      "        }"
      "      }"
      "    </data>"
      "  </app>"
      "</response>";
  ProtocolParserXML xml_parser;
  EXPECT_TRUE(xml_parser.Parse(kUpdateResponse));
  EXPECT_TRUE(xml_parser.errors().empty());

  const update_client::ProtocolParser::Results& results = xml_parser.results();

  // No daystart for offline installs.
  EXPECT_EQ(results.daystart_elapsed_seconds,
            update_client::ProtocolParser::kNoDaystart);
  EXPECT_EQ(results.daystart_elapsed_days,
            update_client::ProtocolParser::kNoDaystart);
  EXPECT_EQ(results.list.size(), size_t{1});

  EXPECT_EQ(results.system_requirements.platform, "win");
  EXPECT_EQ(results.system_requirements.arch, "x64");
  EXPECT_EQ(results.system_requirements.min_os_version, "6.1");

  const update_client::ProtocolParser::Result& result = results.list[0];
  EXPECT_TRUE(result.action_run.empty());
  EXPECT_EQ(result.cohort_attrs.size(), size_t{0});
  EXPECT_EQ(result.custom_attributes.size(), size_t{0});
  EXPECT_EQ(result.crx_diffurls.size(), size_t{0});

  EXPECT_EQ(result.extension_id, "{8A69D345-D564-463C-AFF1-A69D9E530F96}");
  EXPECT_EQ(result.status, "ok");
  EXPECT_EQ(result.crx_urls.size(), size_t{2});
  EXPECT_EQ(result.crx_urls[0], "http://dl.google.com/edgedl/chrome/install/");
  EXPECT_EQ(result.crx_urls[1], "http://dl.google.com/fallback/install/");

  EXPECT_EQ(result.manifest.version, "1.2.3.4");
  EXPECT_TRUE(result.manifest.browser_min_version.empty());
  EXPECT_EQ(result.manifest.run, "installer.exe");
  EXPECT_EQ(result.manifest.arguments, "--do-not-launch-chrome");

  EXPECT_EQ(result.manifest.packages.size(), size_t{1});
  EXPECT_TRUE(result.manifest.packages[0].fingerprint.empty());
  EXPECT_EQ(result.manifest.packages[0].name, "installer.exe");
  EXPECT_EQ(result.manifest.packages[0].hash_sha256, "SamPLE_SHA==");
  EXPECT_EQ(result.manifest.packages[0].size, 112233);
  EXPECT_TRUE(result.manifest.packages[0].namediff.empty());
  EXPECT_TRUE(result.manifest.packages[0].hashdiff_sha256.empty());
  EXPECT_EQ(result.manifest.packages[0].sizediff, 0);

  EXPECT_EQ(result.data.size(), size_t{2});
  EXPECT_EQ(result.data[0].status, "ok");
  EXPECT_EQ(result.data[0].name, "install");
  EXPECT_EQ(result.data[0].install_data_index, "verboselogging");
  EXPECT_EQ(result.data[0].text,
            "{"
            "        \"distribution\": {"
            "          \"verbose_logging\": true"
            "        }"
            "      }");
  EXPECT_EQ(result.data[1].status, "ok");
  EXPECT_EQ(result.data[1].name, "install");
  EXPECT_EQ(result.data[1].install_data_index, "defaultbrowser");
  EXPECT_EQ(result.data[1].text,
            "{"
            "        \"distribution\": {"
            "          \"make_chrome_default_for_user\": true"
            "        }"
            "      }");
}

TEST(ProtocolParserXML, BadXML) {
  ProtocolParserXML xml_parser;
  EXPECT_FALSE(
      xml_parser.Parse("<response protocol=\"3.0\"></App></response>"));
  EXPECT_EQ(xml_parser.errors(), "Load manifest failed: 0x1");
}

TEST(ProtocolParserXML, UnsupportedVersion) {
  ProtocolParserXML xml_parser;
  EXPECT_FALSE(xml_parser.Parse(
      "<response protocol=\"3.1\">"
      "  <app appid=\"{8A69D345-D564-463C-AFF1-A69D9E530F96}\" status=\"ok\" />"
      "</response>"));
  EXPECT_EQ(xml_parser.errors(), "Unsupported protocol version: 3.1");
}

TEST(ProtocolParserXML, ElementOutOfScope) {
  ProtocolParserXML xml_parser;
  EXPECT_FALSE(xml_parser.Parse(
      "<response protocol=\"3.0\">"
      "  <app appid=\"{8A69D345-D564-463C-AFF1-A69D9E530F96}\" status=\"ok\">"
      "  </app>"
      "  <updatecheck status=\"noupdate\"></updatecheck>"
      "</response>"));
  EXPECT_EQ(xml_parser.errors(), "Unrecognized element: updatecheck");
}

TEST(ProtocolParserXML, MissingAttribute) {
  ProtocolParserXML xml_parser;
  EXPECT_FALSE(xml_parser.Parse(
      "<response protocol=\"3.0\"><app status=\"ok\"></app></response>"));
  EXPECT_EQ(xml_parser.errors(), "Missing `appid` attribute in <app>");
}

TEST(ProtocolParserXML, UnrecognizedElement) {
  ProtocolParserXML xml_parser;
  EXPECT_FALSE(
      xml_parser.Parse("<response protocol=\"3.0\"><unknown /></response>"));
  EXPECT_EQ(xml_parser.errors(), "Unrecognized element: unknown");
}

TEST(ProtocolParserXML, BadUrlValue) {
  ProtocolParserXML xml_parser;
  EXPECT_FALSE(xml_parser.Parse(
      "<response protocol=\"3.0\">"
      "  <app appid=\"{8A69D345-D564-463C-AFF1-A69D9E530F96}\" status=\"ok\">"
      "    <updatecheck status=\"ok\">"
      "      <urls>"
      "        <url codebase=\"ht@tp://dl.google.com\"/>"
      "      </urls>"
      "    </updatecheck>"
      "  </app>"
      "</response>"));
  EXPECT_EQ(xml_parser.errors(),
            "Invalid URL codebase in <url>: ht@tp://dl.google.com");
}

TEST(ProtocolParserXML, MissingManifestVersion) {
  ProtocolParserXML xml_parser;
  EXPECT_TRUE(xml_parser.Parse(
      "<response protocol=\"3.0\">"
      "  <app appid=\"{8A69D345-D564-463C-AFF1-A69D9E530F96}\" status=\"ok\">"
      "    <updatecheck status=\"ok\">"
      "      <manifest />"
      "    </updatecheck>"
      "  </app>"
      "</response>"));
  EXPECT_TRUE(xml_parser.errors().empty());
}

TEST(ProtocolParserXML, BadManifestVersion) {
  ProtocolParserXML xml_parser;
  EXPECT_TRUE(xml_parser.Parse(
      "<response protocol=\"3.0\">"
      "  <app appid=\"{8A69D345-D564-463C-AFF1-A69D9E530F96}\" status=\"ok\">"
      "    <updatecheck status=\"ok\">"
      "      <manifest version=\"100.99.0.abc\" />"
      "    </updatecheck>"
      "  </app>"
      "</response>"));
  EXPECT_TRUE(xml_parser.errors().empty());
}

TEST(ProtocolParserXML, BadActionEvent) {
  ProtocolParserXML xml_parser;
  EXPECT_FALSE(xml_parser.Parse(
      "<response protocol=\"3.0\">"
      "  <app appid=\"{8A69D345-D564-463C-AFF1-A69D9E530F96}\" status=\"ok\">"
      "    <updatecheck status=\"ok\">"
      "      <manifest version=\"1.2.3.4\">"
      "        <actions>"
      "          <action event=\"bad_event_type\"/>"
      "        </actions>"
      "      </manifest>"
      "    </updatecheck>"
      "  </app>"
      "</response>"));
  EXPECT_EQ(xml_parser.errors(),
            "Unsupported `event` type in <action>: bad_event_type");
}

TEST(ProtocolParserXML, MulitpleActions) {
  ProtocolParserXML xml_parser;
  EXPECT_TRUE(xml_parser.Parse(
      "<response protocol=\"3.0\">"
      "  <app appid=\"{8A69D345-D564-463C-AFF1-A69D9E530F96}\" status=\"ok\">"
      "    <updatecheck status=\"ok\">"
      "      <manifest version=\"1.2.3.4\">"
      "        <actions>"
      "          <action event=\"install\" run=\"installer.exe\" "
      "              arguments=\"--do-not-launch-chrome\"/>"
      "          <action event=\"install\" run=\"installer2.exe\" "
      "              arguments=\"--do-not-launch-chrome\"/>"
      "        </actions>"
      "      </manifest>"
      "    </updatecheck>"
      "  </app>"
      "</response>"));
  EXPECT_EQ(xml_parser.results().list[0].manifest.run, "installer.exe");
}

}  // namespace updater
