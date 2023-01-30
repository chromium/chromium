// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/redaction_tool/redaction_tool.h"

#include <gtest/gtest.h>
#include <set>
#include <utility>

#include "base/strings/string_util.h"
#include "build/chromeos_buildflags.h"
#include "components/feedback/redaction_tool/pii_types.h"

namespace redaction {

const char kFakeFirstPartyID[] = "nkoccljplnhpfnfiajclkommnmllphnl";
const char* const kFakeFirstPartyExtensionIDs[] = {kFakeFirstPartyID, nullptr};

struct StringWithRedaction {
  // The raw version of the string before redaction. May contain PII sensitive
  // data.
  std::string pre_redaction;
  // The string that's redacted of PII sensitive data.
  std::string post_redaction;
  // The PII type that string contains. PIIType::kNone if the string doesn't
  // contain any PII sensitive data.
  PIIType pii_type;
};

// For better readability, put all the pre/post redaction strings in an array of
// StringWithRedaction struct, and then convert that to two strings which become
// the input and output of the redactor.
const StringWithRedaction kStringsWithRedactions[] = {
    {"aaaaaaaa [SSID=123aaaaaa]aaaaa",  // SSID.
     "aaaaaaaa [SSID=<SSID: 1>]aaaaa", PIIType::kSSID},
    {"aaaaaaaahttp://tets.comaaaaaaa",  // URL.
     "aaaaaaaa<URL: 1>", PIIType::kURL},
    {"u:object_r:system_data_file:s0:c512,c768",  // No PII, it is an SELinux
                                                  // context.
     "u:object_r:system_data_file:s0:c512,c768", PIIType::kNone},
    {"aaaaaemail@example.comaaa",  // Email address.
     "<email: 1>", PIIType::kEmail},
    {"example@@1234",  // No PII, it is not a valid email address.
     "example@@1234", PIIType::kNone},
    {"255.255.155.2",  // IP address.
     "<IPv4: 1>", PIIType::kIPAddress},
    {"255.255.155.255",  // IP address.
     "<IPv4: 2>", PIIType::kIPAddress},
    {"127.0.0.1",  // IPv4 loopback.
     "<127.0.0.0/8: 3>", PIIType::kIPAddress},
    {"127.255.0.1",  // IPv4 loopback.
     "<127.0.0.0/8: 4>", PIIType::kIPAddress},
    {"0.0.0.0",  // Any IPv4.
     "<0.0.0.0/8: 5>", PIIType::kIPAddress},
    {"0.255.255.255",  // Any IPv4.
     "<0.0.0.0/8: 6>", PIIType::kIPAddress},
    {"10.10.10.100",  // IPv4 private class A.
     "<10.0.0.0/8: 7>", PIIType::kIPAddress},
    {"10.10.10.100",  // Intentional duplicate.
     "<10.0.0.0/8: 7>", PIIType::kIPAddress},
    {"10.10.10.101",  // IPv4 private class A.
     "<10.0.0.0/8: 8>", PIIType::kIPAddress},
    {"10.255.255.255",  // IPv4 private class A.
     "<10.0.0.0/8: 9>", PIIType::kIPAddress},
    {"172.16.0.0",  // IPv4 private class B.
     "<172.16.0.0/12: 10>", PIIType::kIPAddress},
    {"172.31.255.255",  // IPv4 private class B.
     "<172.16.0.0/12: 11>", PIIType::kIPAddress},
    {"172.11.5.5",  // IP address.
     "<IPv4: 12>", PIIType::kIPAddress},
    {"172.111.5.5",  // IP address.
     "<IPv4: 13>", PIIType::kIPAddress},
    {"192.168.0.0",  // IPv4 private class C.
     "<192.168.0.0/16: 14>", PIIType::kIPAddress},
    {"192.168.255.255",  // IPv4 private class C.
     "<192.168.0.0/16: 15>", PIIType::kIPAddress},
    {"192.169.2.120",  // IP address.
     "<IPv4: 16>", PIIType::kIPAddress},
    {"169.254.0.1",  // Link local.
     "<169.254.0.0/16: 17>", PIIType::kIPAddress},
    {"169.200.0.1",  // IP address.
     "<IPv4: 18>", PIIType::kIPAddress},
    {"fe80::",  // Link local.
     "<fe80::/10: 1>", PIIType::kIPAddress},
    {"fe80::ffff",  // Link local.
     "<fe80::/10: 2>", PIIType::kIPAddress},
    {"febf:ffff::ffff",  // Link local.
     "<fe80::/10: 3>", PIIType::kIPAddress},
    {"fecc::1111",  // IP address.
     "<IPv6: 4>", PIIType::kIPAddress},
    {"224.0.0.24",  // Multicast.
     "<224.0.0.0/4: 19>", PIIType::kIPAddress},
    {"240.0.0.0",  // IP address.
     "<IPv4: 20>", PIIType::kIPAddress},
    {"255.255.255.255",  // Broadcast.
     "255.255.255.255", PIIType::kNone},
    {"100.115.92.92",  // ChromeOS.
     "100.115.92.92", PIIType::kNone},
    {"100.115.91.92",  // IP address.
     "<IPv4: 21>", PIIType::kIPAddress},
    {"1.1.1.1",  // DNS
     "1.1.1.1", PIIType::kNone},
    {"8.8.8.8",  // DNS
     "8.8.8.8", PIIType::kNone},
    {"8.8.4.4",  // DNS
     "8.8.4.4", PIIType::kNone},
    {"8.8.8.4",  // IP address.
     "<IPv4: 22>", PIIType::kIPAddress},
    {"255.255.259.255",  // Not an IP address.
     "255.255.259.255", PIIType::kNone},
    {"255.300.255.255",  // Not an IP address.
     "255.300.255.255", PIIType::kNone},
    {"3-1.2.3.4",  // USB path, not an IP address.
     "3-1.2.3.4", PIIType::kNone},
    {"Revision: 81600.0000.00.29.19.16_DO",  // Modem firmware
     "Revision: 81600.0000.00.29.19.16_DO", PIIType::kNone},
    {"aaaa123.123.45.4aaa",  // IP address.
     "aaaa<IPv4: 23>aaa", PIIType::kIPAddress},
    {"11:11;11::11",  // IP address.
     "11:11;<IPv6: 5>", PIIType::kIPAddress},
    {"11::11",  // IP address.
     "<IPv6: 5>", PIIType::kIPAddress},
    {"11:11:abcdef:0:0:0:0:0",  // No PII.
     "11:11:abcdef:0:0:0:0:0", PIIType::kNone},
    {"::",  // Unspecified.
     "::", PIIType::kNone},
    {"::1",  // Local host.
     "::1", PIIType::kNone},
    {"Instance::Set",  // Ignore match, no PII.
     "Instance::Set", PIIType::kNone},
    {"Instant::ff",  // Ignore match, no PII.
     "Instant::ff", PIIType::kNone},
    {"net::ERR_CONN_TIMEOUT",  // Ignore match, no PII.
     "net::ERR_CONN_TIMEOUT", PIIType::kNone},
    {"ff01::1",  // All nodes address (interface local).
     "ff01::1", PIIType::kNone},
    {"ff01::2",  // All routers (interface local).
     "ff01::2", PIIType::kNone},
    {"ff01::3",  // Multicast (interface local).
     "<ff01::/16: 6>", PIIType::kIPAddress},
    {"ff02::1",  // All nodes address (link local).
     "ff02::1", PIIType::kNone},
    {"ff02::2",  // All routers (link local).
     "ff02::2", PIIType::kNone},
    {"ff02::3",  // Multicast (link local).
     "<ff02::/16: 7>", PIIType::kIPAddress},
    {"ff02::fb",  // mDNSv6 (link local).
     "<ff02::/16: 8>", PIIType::kIPAddress},
    {"ff08::fb",  // mDNSv6.
     "<IPv6: 9>", PIIType::kIPAddress},
    {"ff0f::101",  // All NTP servers.
     "<IPv6: 10>", PIIType::kIPAddress},
    {"::ffff:cb0c:10ea",  // IPv4-mapped IPV6 (IP address).
     "<IPv6: 11>", PIIType::kIPAddress},
    {"::ffff:a0a:a0a",  // IPv4-mapped IPV6 (private class A).
     "<M 10.0.0.0/8: 12>", PIIType::kIPAddress},
    {"::ffff:a0a:a0a",  // Intentional duplicate.
     "<M 10.0.0.0/8: 12>", PIIType::kIPAddress},
    {"::ffff:ac1e:1e1e",  // IPv4-mapped IPV6 (private class B).
     "<M 172.16.0.0/12: 13>", PIIType::kIPAddress},
    {"::ffff:c0a8:640a",  // IPv4-mapped IPV6 (private class C).
     "<M 192.168.0.0/16: 14>", PIIType::kIPAddress},
    {"::ffff:6473:5c01",  // IPv4-mapped IPV6 (Chrome).
     "<M 100.115.92.1: 15>", PIIType::kIPAddress},
    {"64:ff9b::a0a:a0a",  // IPv4-translated 6to4 IPV6 (private class A).
     "<T 10.0.0.0/8: 16>", PIIType::kIPAddress},
    {"64:ff9b::6473:5c01",  // IPv4-translated 6to4 IPV6 (Chrome).
     "<T 100.115.92.1: 17>", PIIType::kIPAddress},
    {"::0101:ffff:c0a8:640a",  // IP address.
     "<IPv6: 18>", PIIType::kIPAddress},
    {"aa:aa:aa:aa:aa:aa",  // MAC address (BSSID).
     "[MAC OUI=aa:aa:aa IFACE=1]", PIIType::kMACAddress},
    {"chrome://resources/foo",  // Secure chrome resource, exempt.
     "chrome://resources/foo", PIIType::kNone},
    {"chrome://settings/crisper.js",  // Exempt settings URLs.
     "chrome://settings/crisper.js", PIIType::kNone},
    // Exempt first party extension.
    {"chrome-extension://nkoccljplnhpfnfiajclkommnmllphnl/foobar.js",
     "chrome-extension://nkoccljplnhpfnfiajclkommnmllphnl/foobar.js",
     PIIType::kNone},
    {"chrome://resources/f?user=bar",  // Potentially PII in parameter.
     "<URL: 2>", PIIType::kURL},
    {"chrome-extension://nkoccljplnhpfnfiajclkommnmllphnl/foobar.js?bar=x",
     "<URL: 3>", PIIType::kURL},  // Potentially PII in parameter.
    {"isolated-app://airugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac/",
     "<URL: 4>", PIIType::kURL},  // URL
    {"/root/27540283740a0897ab7c8de0f809add2bacde78f/foo",
     "/root/<HASH:2754 1>/foo", PIIType::kStableIdentifier},  // Hash string.
    {"B3mcFTkQAHofv94DDTUuVJGGEI/BbzsyDncplMCR2P4=", "<UID: 1>",
     PIIType::kStableIdentifier},
#if BUILDFLAG(IS_CHROMEOS_ASH)  // We only redact Android paths on Chrome OS.
    // Allowed android storage path.
    {"112K\t/home/root/deadbeef1234/android-data/data/system_de",
     "112K\t/home/root/deadbeef1234/android-data/data/system_de",
     PIIType::kNone},
    // Redacted app-specific storage path.
    {"8.0K\t/home/root/deadbeef1234/android-data/data/data/pa.ckage2/de",
     "8.0K\t/home/root/deadbeef1234/android-data/data/data/pa.ckage2/d_",
     PIIType::kAndroidAppStoragePath},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

class RedactionToolTest : public testing::Test {
 protected:
  std::string RedactMACAddresses(const std::string& input) {
    return redactor_.RedactMACAddresses(input, nullptr);
  }

  std::string RedactHashes(const std::string& input) {
    return redactor_.RedactHashes(input, nullptr);
  }

  std::string RedactAndroidAppStoragePaths(const std::string& input) {
    return redactor_.RedactAndroidAppStoragePaths(input, nullptr);
  }

  std::string RedactCustomPatterns(const std::string& input) {
    return redactor_.RedactAndKeepSelectedCustomPatterns(
        input,
        /*pii_types_to_keep=*/{});
  }

  std::string RedactCustomPatternWithContext(
      const std::string& input,
      const CustomPatternWithAlias& pattern) {
    return redactor_.RedactCustomPatternWithContext(input, pattern, nullptr);
  }

  std::string RedactCustomPatternWithoutContext(
      const std::string& input,
      const CustomPatternWithAlias& pattern) {
    return redactor_.RedactCustomPatternWithoutContext(input, pattern, nullptr);
  }

  RedactionTool redactor_{kFakeFirstPartyExtensionIDs};
};

TEST_F(RedactionToolTest, Redact) {
  EXPECT_EQ("", redactor_.Redact(""));
  EXPECT_EQ("foo\nbar\n", redactor_.Redact("foo\nbar\n"));

  // Make sure MAC address redaction is invoked.
  EXPECT_EQ("[MAC OUI=02:46:8a IFACE=1]",
            redactor_.Redact("02:46:8a:ce:13:57"));

  // Make sure hash redaction is invoked.
  EXPECT_EQ("<HASH:1122 1>",
            redactor_.Redact("11223344556677889900AABBCCDDEEFF"));

  // Make sure custom pattern redaction is invoked.
  EXPECT_EQ("Cell ID: '<CellID: 1>'", RedactCustomPatterns("Cell ID: 'A1B2'"));

  // Make sure UUIDs are redacted.
  EXPECT_EQ(
      "REQUEST localhost - - \"POST /printers/<UUID: 1> HTTP/1.1\" 200 291 "
      "Create-Job successful-ok",
      redactor_.Redact(
          "REQUEST localhost - - \"POST /printers/"
          "cb738a9f-6433-4d95-a81e-94e4ae0ed30b HTTP/1.1\" 200 291 Create-Job "
          "successful-ok"));
  EXPECT_EQ(
      "REQUEST localhost - - \"POST /printers/<UUID: 2> HTTP/1.1\" 200 286 "
      "Create-Job successful-ok",
      redactor_.Redact(
          "REQUEST localhost - - \"POST /printers/"
          "d17188da-9cd3-44f4-b148-3e1d748a3b0f HTTP/1.1\" 200 286 Create-Job "
          "successful-ok"));
}

TEST_F(RedactionToolTest, RedactMACAddresses) {
  EXPECT_EQ("", RedactMACAddresses(""));
  EXPECT_EQ("foo\nbar\n", RedactMACAddresses("foo\nbar\n"));
  EXPECT_EQ("11:22:33:44:55", RedactMACAddresses("11:22:33:44:55"));
  EXPECT_EQ("[MAC OUI=aa:bb:cc IFACE=1]",
            RedactMACAddresses("aa:bb:cc:dd:ee:ff"));
  EXPECT_EQ("[MAC OUI=aa:bb:cc IFACE=1]",
            RedactMACAddresses("aa_bb_cc_dd_ee_ff"));
  EXPECT_EQ("[MAC OUI=aa:bb:cc IFACE=1]",
            RedactMACAddresses("aa-bb-cc-dd-ee-ff"));
  EXPECT_EQ("00:00:00:00:00:00", RedactMACAddresses("00:00:00:00:00:00"));
  EXPECT_EQ("ff:ff:ff:ff:ff:ff", RedactMACAddresses("ff:ff:ff:ff:ff:ff"));
  EXPECT_EQ(
      "BSSID: [MAC OUI=aa:bb:cc IFACE=1] in the middle\n"
      "[MAC OUI=bb:cc:dd IFACE=2] start of line\n"
      "end of line [MAC OUI=aa:bb:cc IFACE=1]\n"
      "no match across lines aa:bb:cc:\n"
      "dd:ee:ff two on the same line:\n"
      "x [MAC OUI=bb:cc:dd IFACE=2] [MAC OUI=cc:dd:ee IFACE=3] x\n",
      RedactMACAddresses("BSSID: aa:bb:cc:dd:ee:ff in the middle\n"
                         "bb:cc:dd:ee:ff:00 start of line\n"
                         "end of line aa:bb:cc:dd:ee:ff\n"
                         "no match across lines aa:bb:cc:\n"
                         "dd:ee:ff two on the same line:\n"
                         "x bb:cc:dd:ee:ff:00 cc:dd:ee:ff:00:11 x\n"));
  EXPECT_EQ("Remember [MAC OUI=bb:cc:dd IFACE=2]?",
            RedactMACAddresses("Remember bB:Cc:DD:ee:ff:00?"));
}

TEST_F(RedactionToolTest, RedactHashes) {
  EXPECT_EQ("", RedactHashes(""));
  EXPECT_EQ("foo\nbar\n", RedactHashes("foo\nbar\n"));
  // Too short.
  EXPECT_EQ("11223344556677889900aabbccddee",
            RedactHashes("11223344556677889900aabbccddee"));
  // Not the right length.
  EXPECT_EQ("11223344556677889900aabbccddeeff1122",
            RedactHashes("11223344556677889900aabbccddeeff1122"));
  // Too long.
  EXPECT_EQ(
      "11223344556677889900aabbccddeeff11223344556677889900aabbccddeeff11",
      RedactHashes("11223344556677889900aabbccddeeff11223344556677889900aabb"
                   "ccddeeff11"));
  // Test all 3 valid lengths.
  EXPECT_EQ("<HASH:aabb 1>", RedactHashes("aabbccddeeff00112233445566778899"));
  EXPECT_EQ("<HASH:aabb 2>",
            RedactHashes("aabbccddeeff00112233445566778899aabbccdd"));
  EXPECT_EQ(
      "<HASH:9988 3>",
      RedactHashes(
          "99887766554433221100ffeeddccbbaaaabbccddeeff00112233445566778899"));
  // Skip 32 byte hashes that have a at least 3 whitespace chars before it.
  EXPECT_EQ("  <HASH:aabb 1>",
            RedactHashes("  aabbccddeeff00112233445566778899"));
  EXPECT_EQ("   aabbccddeeff00112233445566778899",
            RedactHashes("   aabbccddeeff00112233445566778899"));
  // Multiline test.
  EXPECT_EQ(
      "Hash value=<HASH:aabb 1>, should be replaced as\n"
      "well as /<HASH:aabb 1>/ and mixed case of\n"
      "<HASH:aabb 1> but we don't go across lines\n"
      "aabbccddeeff\n00112233445566778899 but allow multiple on a line "
      "<HASH:aabb 4>-"
      "<HASH:0011 5>\n",
      RedactHashes(
          "Hash value=aabbccddeeff00112233445566778899, should be replaced as\n"
          "well as /aabbccddeeff00112233445566778899/ and mixed case of\n"
          "AaBbCCddEeFf00112233445566778899 but we don't go across lines\n"
          "aabbccddeeff\n00112233445566778899 but allow multiple on a line "
          "aabbccddeeffaabbccddeeffaabbccddeeffaabb-"
          "00112233445566778899aabbccddeeff\n"));
}

TEST_F(RedactionToolTest, RedactCustomPatterns) {
  EXPECT_EQ("", RedactCustomPatterns(""));

  EXPECT_EQ("Cell ID: '<CellID: 1>'", RedactCustomPatterns("Cell ID: 'A1B2'"));
  EXPECT_EQ("Cell ID: '<CellID: 2>'", RedactCustomPatterns("Cell ID: 'C1D2'"));
  EXPECT_EQ("foo Cell ID: '<CellID: 1>' bar",
            RedactCustomPatterns("foo Cell ID: 'A1B2' bar"));

  EXPECT_EQ("foo Location area code: '<LocAC: 1>' bar",
            RedactCustomPatterns("foo Location area code: 'A1B2' bar"));

  EXPECT_EQ("foo\na SSID='<SSID: 1>' b\n'",
            RedactCustomPatterns("foo\na SSID='Joe's' b\n'"));
  EXPECT_EQ("ssid '<SSID: 2>'", RedactCustomPatterns("ssid 'My AP'"));
  EXPECT_EQ("bssid 'aa:bb'", RedactCustomPatterns("bssid 'aa:bb'"));

  EXPECT_EQ("Scan SSID - hexdump(len=6): <SSIDHex: 1>\nfoo",
            RedactCustomPatterns(
                "Scan SSID - hexdump(len=6): 47 6f 6f 67 6c 65\nfoo"));

  EXPECT_EQ(
      "a\nb [SSID=<SSID: 3>] [SSID=<SSID: 1>] [SSID=foo\nbar] b",
      RedactCustomPatterns("a\nb [SSID=foo] [SSID=Joe's] [SSID=foo\nbar] b"));
  EXPECT_EQ("ssid=\"<SSID: 4>\"",
            RedactCustomPatterns("ssid=\"LittleTsunami\""));
  EXPECT_EQ("* SSID=<SSID: 5>", RedactCustomPatterns("* SSID=agnagna"));

  EXPECT_EQ("Specifier: <ArcNetworkFactory#1> SSID: \"<SSID: 6>\" foo",
            RedactCustomPatterns(
                "Specifier: <ArcNetworkFactory#1> SSID: \"GoogleGuest1\" foo"));
  EXPECT_EQ("Specifier: <ArcNetworkFactory#1> SSID: '<SSID: 7>' foo",
            RedactCustomPatterns(
                "Specifier: <ArcNetworkFactory#1> SSID: 'GoogleGuest2' foo"));
  EXPECT_EQ("Specifier: <ArcNetworkFactory#1> SSID: <SSID: 8>",
            RedactCustomPatterns(
                "Specifier: <ArcNetworkFactory#1> SSID: GoogleGuest3"));
  EXPECT_EQ(
      "Specifier: <ArcNetworkFactory#1> SSID: <SSID: 9>",
      RedactCustomPatterns(
          "Specifier: <ArcNetworkFactory#1> SSID: less than 32 characters"));
  EXPECT_EQ("Specifier: <ArcNetworkFactory#1> SSID: <SSID: 10>foo",
            RedactCustomPatterns("Specifier: <ArcNetworkFactory#1> SSID: this "
                                 "line is 32 characters long!foo"));
  EXPECT_EQ(
      "<WifiNetworkSpecifier [, SSID Match pattern=PatternMatcher{LITERAL: "
      "<SSID: 11>}, ...]",
      RedactCustomPatterns("<WifiNetworkSpecifier [, SSID Match "
                           "pattern=PatternMatcher{LITERAL: Google-A}, ...]"));

  EXPECT_EQ("SerialNumber: <Serial: 1>",
            RedactCustomPatterns("SerialNumber: 1217D7EF"));
  EXPECT_EQ("serial  number: <Serial: 2>",
            RedactCustomPatterns("serial  number: 50C971FEE7F3x010900"));
  EXPECT_EQ("SerialNumber: <Serial: 3>",
            RedactCustomPatterns("SerialNumber: EVT23-17BA01-004"));
  EXPECT_EQ("serial=\"<Serial: 4>\"",
            RedactCustomPatterns("serial=\"1234AA5678\""));
  EXPECT_EQ("\"serial_number\"=\"<Serial: 1>\"",
            RedactCustomPatterns("\"serial_number\"=\"1217D7EF\""));
  EXPECT_EQ("SerialNumber: <Serial: 5>",
            RedactCustomPatterns("SerialNumber: 5:00:14.0"));
  EXPECT_EQ("Serial: <Serial: 6>",
            RedactCustomPatterns("Serial: ABCEFG\x01kjmn-as:342/234\\432"));
  // Don't overly redact serial numbers, we only do this for a specific
  // formatting case for edid-decode.
  EXPECT_EQ("Foo serial number 123",
            RedactCustomPatterns("Foo serial number 123"));
  EXPECT_EQ("Foo Serial Number <Serial: 7>",
            RedactCustomPatterns("Foo Serial Number 123"));
  // redact serial number separated by a | with the label "serial"
  EXPECT_EQ("serial               | <Serial: 8>",
            RedactCustomPatterns("serial               | 0x1cc04416"));
  EXPECT_EQ("serial               |<Serial: 9>",
            RedactCustomPatterns("serial               |0x1cc04417"));
  EXPECT_EQ("serial|<Serial: 10>", RedactCustomPatterns("serial|0x1cc04418"));
  EXPECT_EQ("serial|<Serial: 11>", RedactCustomPatterns("serial|agnagna"));
  // redact attested device id that is also a serial number
  EXPECT_EQ("\"attested_device_id\"=\"<Serial: 12>\"",
            RedactCustomPatterns("\"attested_device_id\"=\"5CD045B0DZ\""));
  EXPECT_EQ("\"attested_device_id\"=\"<Serial: 13>\"",
            RedactCustomPatterns("\"attested_device_id\"=\"5CD04-5B0DZ\""));
  // The dash cannot appear first or last.
  EXPECT_EQ("\"attested_device_id\"=\"-5CD045B0DZ\"",
            RedactCustomPatterns("\"attested_device_id\"=\"-5CD045B0DZ\""));
  EXPECT_EQ("\"attested_device_id\"=\"5CD045B0DZ-\"",
            RedactCustomPatterns("\"attested_device_id\"=\"5CD045B0DZ-\""));

  // Valid PSM identifiers.
  EXPECT_EQ("PSM id: <PSM ID: 1>", RedactCustomPatterns("PSM id: ABCZ/123xx"));
  EXPECT_EQ("psm: <PSM ID: 2>", RedactCustomPatterns("psm: ABC123F2/123xx"));
  EXPECT_EQ("PsM: <PSM ID: 3>", RedactCustomPatterns("PsM: abcf6677/123xx"));
  EXPECT_EQ("PSM determination successful. Identifier <PSM ID: 4> not present.",
            RedactCustomPatterns("PSM determination successful. Identifier "
                                 "JTFE/223PE6015195 not present."));
  // Wrong number of brand code characters.
  EXPECT_EQ("PSM: ABC/123xx", RedactCustomPatterns("PSM: ABC/123xx"));
  // Non-hex brand code.
  EXPECT_EQ("PSM: zefg0000/123xx", RedactCustomPatterns("PSM: zefg0000/123xx"));
  // No mention of PSM prior to identifier, e.g. in unrelated paths.
  EXPECT_EQ("/root/123xx", RedactCustomPatterns("/root/123xx"));

  EXPECT_EQ("\"gaia_id\":\"<GAIA: 1>\"",
            RedactCustomPatterns("\"gaia_id\":\"1234567890\""));
  EXPECT_EQ("gaia_id='<GAIA: 2>'", RedactCustomPatterns("gaia_id='987654321'"));
  EXPECT_EQ("{id: <GAIA: 1>, email:",
            RedactCustomPatterns("{id: 1234567890, email:"));

  EXPECT_EQ("<email: 1>", RedactCustomPatterns("foo@bar.com"));
  EXPECT_EQ("Email: <email: 1>.", RedactCustomPatterns("Email: foo@bar.com."));
  EXPECT_EQ("Email:\n<email: 2>\n",
            RedactCustomPatterns("Email:\nfooooo@bar.com\n"));

  EXPECT_EQ("[<IPv6: 1>]",
            RedactCustomPatterns("[2001:0db8:0000:0000:0000:ff00:0042:8329]"));
  EXPECT_EQ("[<IPv6: 2>]",
            RedactCustomPatterns("[2001:db8:0:0:0:ff00:42:8329]"));
  EXPECT_EQ("[<IPv6: 3>]", RedactCustomPatterns("[2001:db8::ff00:42:8329]"));
  EXPECT_EQ("[<IPv6: 4>]", RedactCustomPatterns("[aa::bb]"));
  EXPECT_EQ("State::Abort", RedactCustomPatterns("State::Abort"));

  // Real IPv4 address
  EXPECT_EQ("<IPv4: 1>", RedactCustomPatterns("192.160.0.1"));

  // Non-PII IPv4 address (see MaybeScrubIPAddress)
  EXPECT_EQ("255.255.255.255", RedactCustomPatterns("255.255.255.255"));

  // Not an actual IPv4 address
  EXPECT_EQ("75.748.86.91", RedactCustomPatterns("75.748.86.91"));

  // USB Path - not an actual IPv4 Address
  EXPECT_EQ("4-3.3.3.3", RedactCustomPatterns("4-3.3.3.3"));

  // ModemManager modem firmware revisions - not actual IPv4 Addresses
  EXPECT_EQ("Revision: 81600.0000.00.29.19.16_DO",
            RedactCustomPatterns("Revision: 81600.0000.00.29.19.16_DO"));
  EXPECT_EQ("Revision: 11.608.09.01.21",
            RedactCustomPatterns("Revision: 11.608.09.01.21"));
  EXPECT_EQ("Revision: 11.208.09.01.21",
            RedactCustomPatterns("Revision: 11.208.09.01.21"));
  EXPECT_EQ("Revision: BD_3GHAP673A4V1.0.0B02",
            RedactCustomPatterns("Revision: BD_3GHAP673A4V1.0.0B02"));
  EXPECT_EQ("Revision: 2.5.21Hd (Date: Jun 17 2008, Time: 12:30:47)",
            RedactCustomPatterns(
                "Revision: 2.5.21Hd (Date: Jun 17 2008, Time: 12:30:47)"));
  EXPECT_EQ(
      "Revision: 9.5.05.01-02  [2006-10-20 17:19:09]",
      RedactCustomPatterns("Revision: 9.5.05.01-02  [2006-10-20 17:19:09]"));
  EXPECT_EQ("Revision: LQA0021.1.1_M573A",
            RedactCustomPatterns("Revision: LQA0021.1.1_M573A"));
  EXPECT_EQ("Revision: 10.10.10.10",
            RedactCustomPatterns("Revision: 10.10.10.10"));

  EXPECT_EQ("<URL: 1>", RedactCustomPatterns("http://example.com/foo?test=1"));
  EXPECT_EQ("Foo <URL: 2> Bar",
            RedactCustomPatterns("Foo http://192.168.0.1/foo?test=1#123 Bar"));
  const char* kURLs[] = {
      "http://example.com/foo?test=1",
      "http://userid:password@example.com:8080",
      "http://userid:password@example.com:8080/",
      "http://@example.com",
      "http://192.168.0.1",
      "http://192.168.0.1/",
      "http://اختبار.com",
      "http://test.com/foo(bar)baz.html",
      "http://test.com/foo%20bar",
      "ftp://test:tester@test.com",
      "chrome://extensions/",
      "chrome-extension://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/options.html",
      "http://example.com/foo?email=foo@bar.com",
      "rtsp://root@example.com/",
      "https://aaaaaaaaaaaaaaaa.com",
      "file:///var/log/messages",
      "file:///usr/local/home/iby/web%20page%20test.html",
  };
  for (size_t i = 0; i < std::size(kURLs); ++i) {
    SCOPED_TRACE(kURLs[i]);
    std::string got = RedactCustomPatterns(kURLs[i]);
    EXPECT_TRUE(
        base::StartsWith(got, "<URL: ", base::CompareCase::INSENSITIVE_ASCII));
    EXPECT_TRUE(base::EndsWith(got, ">", base::CompareCase::INSENSITIVE_ASCII));
  }
  // Test that "Android:" is not considered a schema with empty hier part.
  EXPECT_EQ("The following applies to Android:",
            RedactCustomPatterns("The following applies to Android:"));
}

TEST_F(RedactionToolTest, RedactCustomPatternWithContext) {
  // The PIIType for the CustomPatternWithAlias is not relevant, only for
  // testing.
  const CustomPatternWithAlias kPattern1 = {"ID", "(\\b(?i)id:? ')(\\d+)(')",
                                            PIIType::kStableIdentifier};
  const CustomPatternWithAlias kPattern2 = {"ID", "(\\b(?i)id=')(\\d+)(')",
                                            PIIType::kStableIdentifier};
  const CustomPatternWithAlias kPattern3 = {"IDG", "(\\b(?i)idg=')(\\d+)(')",
                                            PIIType::kLocationInfo};
  EXPECT_EQ("", RedactCustomPatternWithContext("", kPattern1));
  EXPECT_EQ("foo\nbar\n",
            RedactCustomPatternWithContext("foo\nbar\n", kPattern1));
  EXPECT_EQ("id '<ID: 1>'",
            RedactCustomPatternWithContext("id '2345'", kPattern1));
  EXPECT_EQ("id '<ID: 2>'",
            RedactCustomPatternWithContext("id '1234'", kPattern1));
  EXPECT_EQ("id: '<ID: 2>'",
            RedactCustomPatternWithContext("id: '1234'", kPattern1));
  EXPECT_EQ("ID: '<ID: 1>'",
            RedactCustomPatternWithContext("ID: '2345'", kPattern1));
  EXPECT_EQ("x1 id '<ID: 1>' 1x id '<ID: 2>'\nid '<ID: 1>'\n",
            RedactCustomPatternWithContext(
                "x1 id '2345' 1x id '1234'\nid '2345'\n", kPattern1));
  // Different pattern with same alias should reuse the replacements.
  EXPECT_EQ("id='<ID: 2>'",
            RedactCustomPatternWithContext("id='1234'", kPattern2));
  // Different alias should not reuse replacement from another pattern.
  EXPECT_EQ("idg='<IDG: 1>'",
            RedactCustomPatternWithContext("idg='1234'", kPattern3));
  EXPECT_EQ("x<FOO: 1>z",
            RedactCustomPatternWithContext("xyz", {"FOO", "()(y+)()"}));
}

TEST_F(RedactionToolTest, RedactCustomPatternWithoutContext) {
  // The PIIType for the CustomPatternWithAlias here is not relevant, only for
  // testing.
  CustomPatternWithAlias kPattern = {"pattern", "(o+)", PIIType::kEmail};
  EXPECT_EQ("", RedactCustomPatternWithoutContext("", kPattern));
  EXPECT_EQ("f<pattern: 1>\nf<pattern: 2>z\nf<pattern: 1>l\n",
            RedactCustomPatternWithoutContext("fo\nfooz\nfol\n", kPattern));
}

TEST_F(RedactionToolTest, RedactChunk) {
  std::string redaction_input;
  std::string redaction_output;
  for (const auto& s : kStringsWithRedactions) {
    redaction_input.append(s.pre_redaction).append("\n");
    redaction_output.append(s.post_redaction).append("\n");
  }
  EXPECT_EQ(redaction_output, redactor_.Redact(redaction_input));
}

TEST_F(RedactionToolTest, RedactAndKeepSelected) {
  std::string redaction_input;
  std::string redaction_output;
  for (const auto& s : kStringsWithRedactions) {
    redaction_input.append(s.pre_redaction).append("\n");
    redaction_output.append(s.post_redaction).append("\n");
  }
  // Test RedactAndKeepSelected() with no PII type to keep.
  EXPECT_EQ(redaction_output,
            redactor_.RedactAndKeepSelected(redaction_input, {}));
  // Test RedactAndKeepSelected() by only keeping IP addresses in the redacted
  // output.
  std::string redaction_output_ip;
  for (const auto& s : kStringsWithRedactions) {
    if (s.pii_type == PIIType::kIPAddress) {
      redaction_output_ip.append(s.pre_redaction).append("\n");
    } else {
      redaction_output_ip.append(s.post_redaction).append("\n");
    }
  }
  EXPECT_EQ(redaction_output_ip, redactor_.RedactAndKeepSelected(
                                     redaction_input, {PIIType::kIPAddress}));
  // Test RedactAndKeepSelected() by keeping MAC addresses and hashes in the
  // redacted output. The hashes that URLs and Android storage paths contain
  // will be redacted with the URL or Android storage path that they're part of.
  std::string redaction_output_mac_and_hashes;
  for (const auto& s : kStringsWithRedactions) {
    if (s.pii_type == PIIType::kMACAddress ||
        s.pii_type == PIIType::kStableIdentifier) {
      redaction_output_mac_and_hashes.append(s.pre_redaction).append("\n");
    } else {
      redaction_output_mac_and_hashes.append(s.post_redaction).append("\n");
    }
  }
  EXPECT_EQ(
      redaction_output_mac_and_hashes,
      redactor_.RedactAndKeepSelected(
          redaction_input, {PIIType::kMACAddress, PIIType::kStableIdentifier}));
}

TEST_F(RedactionToolTest, RedactUid) {
  EXPECT_EQ("<UID: 1>",
            redactor_.RedactAndKeepSelected(
                "B3mcFTkQAHofv94DDTUuVJGGEI/BbzsyDncplMCR2P4=", {}));
}

TEST_F(RedactionToolTest, RedactAndKeepSelectedHashes) {
  // Array of pairs containing pre/post redaction versions of the same string.
  // Will be appended to create input and expected output for the test. Keep
  // URLs and Android app storage paths but redact hashes. URLs and Android app
  // storage paths that contain hashes will be partially redacted.
  const std::pair<std::string, std::string> redaction_strings_with_hashes[] = {
    {"chrome://resources/"
     "f?user="
     "99887766554433221100ffeeddccbbaaaabbccddeeff00112233445566778899",
     "chrome://resources/f?user=<HASH:9988 1>"},  // URL that contains a hash.
    {"/root/27540283740a0897ab7c8de0f809add2bacde78f/foo",
     "/root/<HASH:2754 2>/foo"},  // String that contains a hash.
    {"this is the user hash that we need to redact "
     "aabbccddeeff00112233445566778899",
     "this is the user hash that we need to redact <HASH:aabb 3>"},  // String
                                                                     // that
                                                                     // contains
                                                                     // a hash.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"8.0K\t/home/root/aabbccddeeff00112233445566778899/"
     "android-data/data/data/pa.ckage2/de",  // Android app storage
                                             // path that contains a
                                             // hash.
     "8.0K\t/home/root/<HASH:aabb 3>/android-data/data/data/pa.ckage2/de"}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  };
  std::string redaction_input;
  std::string redaction_output;
  for (const auto& s : redaction_strings_with_hashes) {
    redaction_input.append(s.first).append("\n");
    redaction_output.append(s.second).append("\n");
  }
  EXPECT_EQ(
      redaction_output,
      redactor_.RedactAndKeepSelected(
          redaction_input, {PIIType::kAndroidAppStoragePath, PIIType::kURL}));
}

TEST_F(RedactionToolTest, DetectPII) {
  std::string redaction_input;
  for (const auto& s : kStringsWithRedactions) {
    redaction_input.append(s.pre_redaction).append("\n");
  }
  std::map<PIIType, std::set<std::string>> pii_in_data {
#if BUILDFLAG(IS_CHROMEOS_ASH)  // We only detect Android paths on Chrome OS.
    {PIIType::kAndroidAppStoragePath, {"/de"}},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
        {PIIType::kSSID, {"123aaaaaa"}},
        {PIIType::kURL,
         {"http://tets.comaaaaaaa",
          "isolated-app://"
          "airugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac/",
          "chrome://resources/f?user=bar",
          "chrome-extension://nkoccljplnhpfnfiajclkommnmllphnl/"
          "foobar.js?bar=x"}},
        {PIIType::kEmail, {"aaaaaemail@example.comaaa"}},
        {PIIType::kIPAddress,
         {
             "255.255.155.2",
             "255.255.155.255",
             "127.0.0.1",
             "127.255.0.1",
             "0.0.0.0",
             "0.255.255.255",
             "10.10.10.100",
             "10.10.10.101",
             "10.255.255.255",
             "172.16.0.0",
             "172.31.255.255",
             "172.11.5.5",
             "172.111.5.5",
             "192.168.0.0",
             "192.168.255.255",
             "192.169.2.120",
             "169.254.0.1",
             "169.200.0.1",
             "224.0.0.24",
             "240.0.0.0",
             "100.115.91.92",
             "8.8.8.4",
             "123.123.45.4",
             "fe80::",
             "fe80::ffff",
             "febf:ffff::ffff",
             "fecc::1111",
             "11::11",
             "ff01::3",
             "ff02::3",
             "ff02::fb",
             "ff08::fb",
             "ff0f::101",
             "::ffff:cb0c:10ea",
             "::ffff:a0a:a0a",
             "::ffff:ac1e:1e1e",
             "::ffff:c0a8:640a",
             "::ffff:6473:5c01",
             "64:ff9b::a0a:a0a",
             "64:ff9b::6473:5c01",
             "::0101:ffff:c0a8:640a",
         }},
        {PIIType::kMACAddress, {"aa:aa:aa:aa:aa:aa"}}, {
      PIIType::kStableIdentifier, {
        "27540283740a0897ab7c8de0f809add2bacde78f",
            "B3mcFTkQAHofv94DDTUuVJGGEI/BbzsyDncplMCR2P4=",
      }
    }
  };

  EXPECT_EQ(pii_in_data, redactor_.Detect(redaction_input));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)  // We only redact Android paths on Chrome OS.
TEST_F(RedactionToolTest, RedactAndroidAppStoragePaths) {
  EXPECT_EQ("", RedactAndroidAppStoragePaths(""));
  EXPECT_EQ("foo\nbar\n", RedactAndroidAppStoragePaths("foo\nbar\n"));

  constexpr char kDuOutput[] =
      "112K\t/home/root/deadbeef1234/android-data/data/system_de\n"
      // /data/data will be modified by the redactor.
      "8.0K\t/home/root/deadbeef1234/android-data/data/data/pack.age1/a\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/data/pack.age1/bc\n"
      "24K\t/home/root/deadbeef1234/android-data/data/data/pack.age1\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/data/pa.ckage2/de\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/data/pa.ckage2/de/"
      "\xe3\x81\x82\n"
      "8.1K\t/home/root/deadbeef1234/android-data/data/data/pa.ckage2/de/"
      "\xe3\x81\x82\xe3\x81\x83\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/data/pa.ckage2/ef\n"
      "24K\t/home/root/deadbeef1234/android-data/data/data/pa.ckage2\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/app/pack.age1/a\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/app/pack.age1/bc\n"
      "24K\t/home/root/deadbeef1234/android-data/data/app/pack.age1\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/user_de/0/pack.age1/a\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/user_de/0/pack.age1/bc\n"
      "24K\t/home/root/deadbeef1234/android-data/data/user_de/0/pack.age1\n"
      "78M\t/home/root/deadbeef1234/android-data/data/data\n"
      "key=value path=/data/data/pack.age1/bc key=value\n"
      "key=value path=/data/user_de/0/pack.age1/bc key=value\n"
      "key=value exe=/data/app/pack.age1/bc key=value\n";
  constexpr char kDuOutputRedacted[] =
      "112K\t/home/root/deadbeef1234/android-data/data/system_de\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/data/pack.age1/a\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/data/pack.age1/b_\n"
      "24K\t/home/root/deadbeef1234/android-data/data/data/pack.age1\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/data/pa.ckage2/d_\n"
      // The non-ASCII directory names will become '*_'.
      "8.0K\t/home/root/deadbeef1234/android-data/data/data/pa.ckage2/d_/*_\n"
      "8.1K\t/home/root/deadbeef1234/android-data/data/data/pa.ckage2/d_/*_\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/data/pa.ckage2/e_\n"
      "24K\t/home/root/deadbeef1234/android-data/data/data/pa.ckage2\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/app/pack.age1/a\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/app/pack.age1/b_\n"
      "24K\t/home/root/deadbeef1234/android-data/data/app/pack.age1\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/user_de/0/pack.age1/a\n"
      "8.0K\t/home/root/deadbeef1234/android-data/data/user_de/0/pack.age1/b_\n"
      "24K\t/home/root/deadbeef1234/android-data/data/user_de/0/pack.age1\n"
      "78M\t/home/root/deadbeef1234/android-data/data/data\n"
      "key=value path=/data/data/pack.age1/b_ key=value\n"
      "key=value path=/data/user_de/0/pack.age1/b_ key=value\n"
      "key=value exe=/data/app/pack.age1/b_ key=value\n";
  EXPECT_EQ(kDuOutputRedacted, RedactAndroidAppStoragePaths(kDuOutput));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(RedactionToolTest, RedactBlockDevices) {
  // Test cases in the form {input, output}.
  std::pair<std::string, std::string> test_cases[] = {
      // UUIDs that come from the 'blkid' tool.
      {"PTUUID=\"985dff64-9c0f-3f49-945b-2d8c2e0238ec\"",
       "PTUUID=\"<UUID: 1>\""},
      {"UUID=\"E064-868C\"", "UUID=\"<UUID: 2>\""},
      {"PARTUUID=\"7D242B2B1C751832\"", "PARTUUID=\"<UUID: 3>\""},

      // Volume labels.
      {"LABEL=\"ntfs\"", "LABEL=\"<Volume Label: 1>\""},
      {"PARTLABEL=\"SD Card\"", "PARTLABEL=\"<Volume Label: 2>\""},

      // LVM UUIDd.
      {"{\"pv_fmt\":\"lvm2\", "
       "\"pv_uuid\":\"duD18x-P7QE-sTya-SaeO-aq07-YgEq-xj8UEz\", "
       "\"dev_size\":\"230.33g\"}",
       "{\"pv_fmt\":\"lvm2\", \"pv_uuid\":\"<UUID: 4>\", "
       "\"dev_size\":\"230.33g\"}"},
      {"{\"lv_uuid\":\"lKYORl-TWDP-OFLT-yDnB-jlQ7-aQrE-AwA8Oa\", "
       "\"lv_name\":\"[thinpool_tdata]\"",
       "{\"lv_uuid\":\"<UUID: 5>\", \"lv_name\":\"[thinpool_tdata]\""},

      // Removable media paths.
      {"/media/removable/SD Card/", "/media/removable/<Volume Label: 2>/"},
      {"'/media/removable/My Secret Volume Name' don't redact this",
       "'/media/removable/<Volume Label: 3>' don't redact this"},
      {"0 part /media/removable/My Secret Volume Name         With Spaces   ",
       "0 part /media/removable/<Volume Label: 4>"},
  };
  for (const auto& p : test_cases) {
    EXPECT_EQ(redactor_.Redact(p.first), p.second);
  }
}

}  // namespace redaction
