// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/protocol_parser_xml.h"

#include <optional>

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

  std::optional<ProtocolParserXML::Results> results =
      ProtocolParserXML::Parse(kUpdateResponse);
  EXPECT_TRUE(results);

  // No daystart for offline installs.
  EXPECT_EQ(results->apps.size(), size_t{1});

  EXPECT_EQ(results->system_requirements.platform, "win");
  EXPECT_EQ(results->system_requirements.arch, "x64");
  EXPECT_EQ(results->system_requirements.min_os_version, "6.1");

  const ProtocolParserXML::App& result = results->apps[0];
  EXPECT_EQ(result.app_id, "{8A69D345-D564-463C-AFF1-A69D9E530F96}");
  EXPECT_EQ(result.manifest.version, "1.2.3.4");
  EXPECT_EQ(result.manifest.run, "installer.exe");
  EXPECT_EQ(result.manifest.arguments, "--do-not-launch-chrome");
  EXPECT_EQ(result.data.size(), size_t{2});
  EXPECT_EQ(result.data[0].install_data_index, "verboselogging");
  EXPECT_EQ(result.data[0].text,
            "{"
            "        \"distribution\": {"
            "          \"verbose_logging\": true"
            "        }"
            "      }");
  EXPECT_EQ(result.data[1].install_data_index, "defaultbrowser");
  EXPECT_EQ(result.data[1].text,
            "{"
            "        \"distribution\": {"
            "          \"make_chrome_default_for_user\": true"
            "        }"
            "      }");
}

TEST(ProtocolParserXML, BadXML) {
  EXPECT_FALSE(
      ProtocolParserXML::Parse("<response protocol=\"3.0\"></App></response>"));
}

TEST(ProtocolParserXML, UnsupportedVersion) {
  EXPECT_FALSE(ProtocolParserXML::Parse(
      "<response protocol=\"3.1\">"
      "  <app appid=\"{8A69D345-D564-463C-AFF1-A69D9E530F96}\" status=\"ok\" />"
      "</response>"));
}

TEST(ProtocolParserXML, MissingAttribute) {
  EXPECT_FALSE(ProtocolParserXML::Parse(
      "<response protocol=\"3.0\"><app status=\"ok\"></app></response>"));
}

TEST(ProtocolParserXML, UnrecognizedElement) {
  EXPECT_TRUE(ProtocolParserXML::Parse(
      "<response protocol=\"3.0\"><unknown /></response>"));
}

TEST(ProtocolParserXML, MissingManifestVersion) {
  EXPECT_TRUE(ProtocolParserXML::Parse(
      "<response protocol=\"3.0\">"
      "  <app appid=\"{8A69D345-D564-463C-AFF1-A69D9E530F96}\" status=\"ok\">"
      "    <updatecheck status=\"ok\">"
      "      <manifest />"
      "    </updatecheck>"
      "  </app>"
      "</response>"));
}

TEST(ProtocolParserXML, BadManifestVersion) {
  EXPECT_TRUE(ProtocolParserXML::Parse(
      "<response protocol=\"3.0\">"
      "  <app appid=\"{8A69D345-D564-463C-AFF1-A69D9E530F96}\" status=\"ok\">"
      "    <updatecheck status=\"ok\">"
      "      <manifest version=\"100.99.0.abc\" />"
      "    </updatecheck>"
      "  </app>"
      "</response>"));
}

TEST(ProtocolParserXML, BadActionEvent) {
  EXPECT_FALSE(ProtocolParserXML::Parse(
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
}

TEST(ProtocolParserXML, MulitpleActions) {
  std::optional<ProtocolParserXML::Results> results = ProtocolParserXML::Parse(
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
      "</response>");
  ASSERT_TRUE(results);
  EXPECT_EQ(results->apps[0].manifest.run, "installer.exe");
}

}  // namespace updater
