// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/feedback/redaction_tool/redaction_tool.h"

#include <gtest/gtest.h>

#include <set>
#include <string_view>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "build/chromeos_buildflags.h"
#include "components/feedback/redaction_tool/metrics_tester.h"
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
     "aaaaaaaa [SSID=(SSID: 1)]aaaaa", PIIType::kSSID},
    {"chrome://resources/foo",  // Secure chrome resource, exempt.
     "chrome://resources/foo", PIIType::kNone},
    {"chrome://settings/crisper.js",  // Exempt settings URLs.
     "chrome://settings/crisper.js", PIIType::kNone},
    // Exempt first party extension.
    {"chrome-extension://nkoccljplnhpfnfiajclkommnmllphnl/foobar.js",
     "chrome-extension://nkoccljplnhpfnfiajclkommnmllphnl/foobar.js",
     PIIType::kNone},
    {"aaaaaaaahttp://tets.comaaaaaaa",  // URL.
     "aaaaaaaa(URL: 1)", PIIType::kURL},
    {"chrome://resources/f?user=bar",  // Potentially PII in parameter.
     "(URL: 2)", PIIType::kURL},
    {"chrome-extension://nkoccljplnhpfnfiajclkommnmllphnl/foobar.js?bar=x",
     "(URL: 3)", PIIType::kURL},  // Potentially PII in parameter.
    {"isolated-app://airugqztij5biqquuk3mfwpsaibuegaqcitgfchwuosuofdjabzqaaac/",
     "(URL: 4)", PIIType::kURL},                  // URL
    {"u:object_r:system_data_file:s0:c512,c768",  // No PII, it is an SELinux
                                                  // context.
     "u:object_r:system_data_file:s0:c512,c768", PIIType::kNone},
    {"aaaaaemail@example.comaaa",  // Email address.
     "(email: 1)", PIIType::kEmail},
    {"example@@1234",  // No PII, it is not a valid email address.
     "example@@1234", PIIType::kNone},
    {"255.255.155.2",  // IP address.
     "(IPv4: 1)", PIIType::kIPAddress},
    {"255.255.155.255",  // IP address.
     "(IPv4: 2)", PIIType::kIPAddress},
    {"127.0.0.1",  // IPv4 loopback.
     "(127.0.0.0/8: 3)", PIIType::kIPAddress},
    {"127.255.0.1",  // IPv4 loopback.
     "(127.0.0.0/8: 4)", PIIType::kIPAddress},
    {"0.0.0.0",  // Any IPv4.
     "(0.0.0.0/8: 5)", PIIType::kIPAddress},
    {"0.255.255.255",  // Any IPv4.
     "(0.0.0.0/8: 6)", PIIType::kIPAddress},
    {"10.10.10.100",  // IPv4 private class A.
     "(10.0.0.0/8: 7)", PIIType::kIPAddress},
    {"10.10.10.100",  // Intentional duplicate.
     "(10.0.0.0/8: 7)", PIIType::kIPAddress},
    {"10.10.10.101",  // IPv4 private class A.
     "(10.0.0.0/8: 8)", PIIType::kIPAddress},
    {"10.255.255.255",  // IPv4 private class A.
     "(10.0.0.0/8: 9)", PIIType::kIPAddress},
    {"172.16.0.0",  // IPv4 private class B.
     "(172.16.0.0/12: 10)", PIIType::kIPAddress},
    {"172.31.255.255",  // IPv4 private class B.
     "(172.16.0.0/12: 11)", PIIType::kIPAddress},
    {"172.11.5.5",  // IP address.
     "(IPv4: 12)", PIIType::kIPAddress},
    {"172.111.5.5",  // IP address.
     "(IPv4: 13)", PIIType::kIPAddress},
    {"192.168.0.0",  // IPv4 private class C.
     "(192.168.0.0/16: 14)", PIIType::kIPAddress},
    {"192.168.255.255",  // IPv4 private class C.
     "(192.168.0.0/16: 15)", PIIType::kIPAddress},
    {"192.169.2.120",  // IP address.
     "(IPv4: 16)", PIIType::kIPAddress},
    {"169.254.0.1",  // Link local.
     "(169.254.0.0/16: 17)", PIIType::kIPAddress},
    {"169.200.0.1",  // IP address.
     "(IPv4: 18)", PIIType::kIPAddress},
    {"fe80::",  // Link local.
     "(fe80::/10: 1)", PIIType::kIPAddress},
    {"fe80::ffff",  // Link local.
     "(fe80::/10: 2)", PIIType::kIPAddress},
    {"febf:ffff::ffff",  // Link local.
     "(fe80::/10: 3)", PIIType::kIPAddress},
    {"fecc::1111",  // IP address.
     "(IPv6: 4)", PIIType::kIPAddress},
    {"224.0.0.24",  // Multicast.
     "(224.0.0.0/4: 19)", PIIType::kIPAddress},
    {"240.0.0.0",  // IP address.
     "(IPv4: 20)", PIIType::kIPAddress},
    {"255.255.255.255",  // Broadcast.
     "255.255.255.255", PIIType::kNone},
    {"100.115.92.92",  // ChromeOS.
     "100.115.92.92", PIIType::kNone},
    {"100.115.91.92",  // IP address.
     "(IPv4: 21)", PIIType::kIPAddress},
    {"1.1.1.1",  // DNS
     "1.1.1.1", PIIType::kNone},
    {"8.8.8.8",  // DNS
     "8.8.8.8", PIIType::kNone},
    {"8.8.4.4",  // DNS
     "8.8.4.4", PIIType::kNone},
    {"8.8.8.4",  // IP address.
     "(IPv4: 22)", PIIType::kIPAddress},
    {"255.255.259.255",  // Not an IP address.
     "255.255.259.255", PIIType::kNone},
    {"255.300.255.255",  // Not an IP address.
     "255.300.255.255", PIIType::kNone},
    {"3-1.2.3.4",  // USB path, not an IP address.
     "3-1.2.3.4", PIIType::kNone},
    {"Revision: 81600.0000.00.29.19.16_DO",  // Modem firmware
     "Revision: 81600.0000.00.29.19.16_DO", PIIType::kNone},
    {"aaaa123.123.45.4aaa",  // IP address.
     "aaaa(IPv4: 23)aaa", PIIType::kIPAddress},
    {"11:11;11::11",  // IP address.
     "11:11;(IPv6: 5)", PIIType::kIPAddress},
    {"11::11",  // IP address.
     "(IPv6: 5)", PIIType::kIPAddress},
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
     "(ff01::/16: 6)", PIIType::kIPAddress},
    {"ff02::1",  // All nodes address (link local).
     "ff02::1", PIIType::kNone},
    {"ff02::2",  // All routers (link local).
     "ff02::2", PIIType::kNone},
    {"ff02::3",  // Multicast (link local).
     "(ff02::/16: 7)", PIIType::kIPAddress},
    {"ff02::fb",  // mDNSv6 (link local).
     "(ff02::/16: 8)", PIIType::kIPAddress},
    {"ff08::fb",  // mDNSv6.
     "(IPv6: 9)", PIIType::kIPAddress},
    {"ff0f::101",  // All NTP servers.
     "(IPv6: 10)", PIIType::kIPAddress},
    {"::ffff:cb0c:10ea",  // IPv4-mapped IPV6 (IP address).
     "(IPv6: 11)", PIIType::kIPAddress},
    {"::ffff:a0a:a0a",  // IPv4-mapped IPV6 (private class A).
     "(M 10.0.0.0/8: 12)", PIIType::kIPAddress},
    {"::ffff:a0a:a0a",  // Intentional duplicate.
     "(M 10.0.0.0/8: 12)", PIIType::kIPAddress},
    {"::ffff:ac1e:1e1e",  // IPv4-mapped IPV6 (private class B).
     "(M 172.16.0.0/12: 13)", PIIType::kIPAddress},
    {"::ffff:c0a8:640a",  // IPv4-mapped IPV6 (private class C).
     "(M 192.168.0.0/16: 14)", PIIType::kIPAddress},
    {"::ffff:6473:5c01",  // IPv4-mapped IPV6 (Chrome).
     "(M 100.115.92.1: 15)", PIIType::kIPAddress},
    {"64:ff9b::a0a:a0a",  // IPv4-translated 6to4 IPV6 (private class A).
     "(T 10.0.0.0/8: 16)", PIIType::kIPAddress},
    {"64:ff9b::6473:5c01",  // IPv4-translated 6to4 IPV6 (Chrome).
     "(T 100.115.92.1: 17)", PIIType::kIPAddress},
    {"::0101:ffff:c0a8:640a",  // IP address.
     "(IPv6: 18)", PIIType::kIPAddress},
    {"aa:aa:aa:aa:aa:aa",  // MAC address (BSSID).
     "(MAC OUI=aa:aa:aa IFACE=1)", PIIType::kMACAddress},
    {"/root/27540283740a0897ab7c8de0f809add2bacde78f/foo",
     "/root/(HASH:2754 1)/foo", PIIType::kStableIdentifier},  // Hash string.
    {"B3mcFTkQAHofv94DDTUuVJGGEI/BbzsyDncplMCR2P4=", "(UID: 1)",
     PIIType::kStableIdentifier},
    {"foo=4012-8888-8888-1881", "foo=(CREDITCARD: 1)", PIIType::kCreditCard},
    {"foo=40 12 88 88 88 88 18 81", "foo=(CREDITCARD: 1)",
     PIIType::kCreditCard},
    // Max length
    {"foo=5019717010103742", "foo=(CREDITCARD: 2)", PIIType::kCreditCard},
    {"foo=5-0-1-9-7-1-7-0-1-0-1-0-3-7-4-2-7-8-7", "foo=(CREDITCARD: 3)",
     PIIType::kCreditCard},
    // Too long to match.
    {"foo=5-0-1-9-7-1-7-0-1-0-1-0-3-7-4-2-7-8-7-2",
     "foo=5-0-1-9-7-1-7-0-1-0-1-0-3-7-4-2-7-8-7-2", PIIType::kNone},
    // Number is too long.
    {"foo=12345678901234567894", "foo=12345678901234567894", PIIType::kNone},
    // Number is too short.
    {"foo=12345678903", "foo=12345678903", PIIType::kNone},
    // Luhn checksum doesn't validate.
    {"foo=4111 1111 1111 1112", "foo=4111 1111 1111 1112", PIIType::kNone},
    // This is probably just a timestamp.
    {"foo=4012888888881881ms", "foo=4012888888881881ms", PIIType::kNone},
    // This is probably just a timestamp as well.
    {"foo=4012888888881881 ms", "foo=4012888888881881 ms", PIIType::kNone},
    // Probably a log entry.
    {"foo=12:00:00.359  1000   155   155 INFO",
     "foo=12:00:00.359  1000   155   155 INFO", PIIType::kNone},
    // Invalid IIN.
    {"foo=0000-0000-0000-0000", "foo=0000-0000-0000-0000", PIIType::kNone},
    // This is not a timestamp even though "ms" appears after the number.
    {"Use 4012888888881881 or moms creditcard",
     "Use (CREDITCARD: 1)or moms creditcard", PIIType::kCreditCard},
    {":GB82 WEST 1234 5698 7654 32", ":(IBAN: 1)", PIIType::kIBAN},
    {":GB33BUKB20201555555555", ":(IBAN: 2)", PIIType::kIBAN},
    {":GB82-WEST-1234-5698-7654-32", ":(IBAN: 1)", PIIType::kIBAN},
    // Invalid check digits.
    {":GB94BARC20201530093459", ":GB94BARC20201530093459", PIIType::kNone},
    // Country does not seem to support IBAN.
    {":US64SVBKUS6S3300958879", ":US64SVBKUS6S3300958879", PIIType::kNone},
    // Random data before a valid IBAN shouldn't match.
    {"base64Data=GB82WEST12345698765432", "base64Data=GB82WEST12345698765432",
     PIIType::kNone},
    {"base64DataGB82WEST12345698765432", "base64DataGB82WEST12345698765432",
     PIIType::kNone},
    // Random data after a valid IBAN shouldn't match.
    {":GB82 WEST 1234 5698 7654 32/base64Data",
     ":GB82 WEST 1234 5698 7654 32/base64Data", PIIType::kNone},
    {":GB82 WEST 1234 5698 7654 32+base64Data",
     ":GB82 WEST 1234 5698 7654 32+base64Data", PIIType::kNone},
    {":GB82 WEST 1234 5698 7654 32=base64Data",
     ":GB82 WEST 1234 5698 7654 32=base64Data", PIIType::kNone},
    {":GB82 WEST 1234 5698 7654 32base64Data",
     ":GB82 WEST 1234 5698 7654 32base64Data", PIIType::kNone},
    // Random data before and after a valid IBAN shouldn't match.
    {"base64DataGB82-WEST-1234-5698-7654-32+base64Data",
     "base64DataGB82-WEST-1234-5698-7654-32+base64Data", PIIType::kNone},
    // Redacted Crash IDs.
    {"Crash report receipt ID 153c963587d8d8d4",
     "Crash report receipt ID (Crash ID: 1)", PIIType::kCrashId},
    {"with prefixCrash report receipt ID 153C963587D8D8D4b with trailing text",
     "with prefixCrash report receipt ID (Crash ID: 2) with trailing text",
     PIIType::kCrashId},
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
 public:
  RedactionToolTest()
      : metrics_tester_(MetricsTester::Create()),
        redactor_(kFakeFirstPartyExtensionIDs,
                  metrics_tester_->SetupRecorder()) {}

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

  template <typename T>
  void ExpectBucketCount(
      std::string_view histogram_name,
      const T enum_value,
      const size_t expected_count,
      const base::Location location = base::Location::Current()) {
    const size_t actual_count = metrics_tester_->GetBucketCount(
        histogram_name, static_cast<int>(enum_value));

    EXPECT_EQ(actual_count, expected_count)
        << location.file_name() << ":" << location.line_number();
  }

  std::unique_ptr<MetricsTester> metrics_tester_;
  RedactionTool redactor_;
};

TEST_F(RedactionToolTest, Redact) {
  EXPECT_EQ("", redactor_.Redact(""));
  EXPECT_EQ("foo\nbar\n", redactor_.Redact("foo\nbar\n"));

  // Make sure MAC address redaction is invoked.
  EXPECT_EQ("(MAC OUI=02:46:8a IFACE=1)",
            redactor_.Redact("02:46:8a:ce:13:57"));

  // Make sure hash redaction is invoked.
  EXPECT_EQ("(HASH:1122 1)",
            redactor_.Redact("11223344556677889900AABBCCDDEEFF"));

  // Make sure (partial) user id hash in cryptohome devices is redacted.
  EXPECT_EQ("dmcrypt-(UID: 1)-cache",
            redactor_.Redact("dmcrypt-123abcde-cache"));
  EXPECT_EQ("FOO-cryptohome--(UID: 1)--cache",
            redactor_.Redact("FOO-cryptohome--123abcde--cache"));

  // Make sure custom pattern redaction is invoked.
  EXPECT_EQ("Cell ID: '(CellID: 1)'", RedactCustomPatterns("Cell ID: 'A1B2'"));

  // Make sure UUIDs are redacted.
  EXPECT_EQ(
      "REQUEST localhost - - \"POST /printers/(UUID: 1) HTTP/1.1\" 200 291 "
      "Create-Job successful-ok",
      redactor_.Redact(
          "REQUEST localhost - - \"POST /printers/"
          "cb738a9f-6433-4d95-a81e-94e4ae0ed30b HTTP/1.1\" 200 291 Create-Job "
          "successful-ok"));
  EXPECT_EQ(
      "REQUEST localhost - - \"POST /printers/(UUID: 2) HTTP/1.1\" 200 286 "
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
  EXPECT_EQ("(MAC OUI=aa:bb:cc IFACE=1)",
            RedactMACAddresses("aa:bb:cc:dd:ee:ff"));
  EXPECT_EQ("(MAC OUI=aa:bb:cc IFACE=1)",
            RedactMACAddresses("aa_bb_cc_dd_ee_ff"));
  EXPECT_EQ("(MAC OUI=aa:bb:cc IFACE=1)",
            RedactMACAddresses("aa-bb-cc-dd-ee-ff"));
  EXPECT_EQ("00:00:00:00:00:00", RedactMACAddresses("00:00:00:00:00:00"));
  EXPECT_EQ("ff:ff:ff:ff:ff:ff", RedactMACAddresses("ff:ff:ff:ff:ff:ff"));
  EXPECT_EQ(
      "BSSID: (MAC OUI=aa:bb:cc IFACE=1) in the middle\n"
      "(MAC OUI=bb:cc:dd IFACE=2) start of line\n"
      "end of line (MAC OUI=aa:bb:cc IFACE=1)\n"
      "no match across lines aa:bb:cc:\n"
      "dd:ee:ff two on the same line:\n"
      "x (MAC OUI=bb:cc:dd IFACE=2) (MAC OUI=cc:dd:ee IFACE=3) x\n",
      RedactMACAddresses("BSSID: aa:bb:cc:dd:ee:ff in the middle\n"
                         "bb:cc:dd:ee:ff:00 start of line\n"
                         "end of line aa:bb:cc:dd:ee:ff\n"
                         "no match across lines aa:bb:cc:\n"
                         "dd:ee:ff two on the same line:\n"
                         "x bb:cc:dd:ee:ff:00 cc:dd:ee:ff:00:11 x\n"));
  EXPECT_EQ("Remember (MAC OUI=bb:cc:dd IFACE=2)?",
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
  EXPECT_EQ("(HASH:aabb 1)", RedactHashes("aabbccddeeff00112233445566778899"));
  EXPECT_EQ("(HASH:aabb 2)",
            RedactHashes("aabbccddeeff00112233445566778899aabbccdd"));
  EXPECT_EQ(
      "(HASH:9988 3)",
      RedactHashes(
          "99887766554433221100ffeeddccbbaaaabbccddeeff00112233445566778899"));
  // Skip 32 byte hashes that have a at least 3 whitespace chars before it.
  EXPECT_EQ("  (HASH:aabb 1)",
            RedactHashes("  aabbccddeeff00112233445566778899"));
  EXPECT_EQ("   aabbccddeeff00112233445566778899",
            RedactHashes("   aabbccddeeff00112233445566778899"));
  // Multiline test.
  EXPECT_EQ(
      "Hash value=(HASH:aabb 1), should be replaced as\n"
      "well as /(HASH:aabb 1)/ and mixed case of\n"
      "(HASH:aabb 1) but we don't go across lines\n"
      "aabbccddeeff\n00112233445566778899 but allow multiple on a line "
      "(HASH:aabb 4)-"
      "(HASH:0011 5)\n",
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

  EXPECT_EQ("Cell ID: '(CellID: 1)'", RedactCustomPatterns("Cell ID: 'A1B2'"));
  EXPECT_EQ("Cell ID: '(CellID: 2)'", RedactCustomPatterns("Cell ID: 'C1D2'"));
  EXPECT_EQ("foo Cell ID: '(CellID: 1)' bar",
            RedactCustomPatterns("foo Cell ID: 'A1B2' bar"));

  EXPECT_EQ("foo Location area code: '(LocAC: 1)' bar",
            RedactCustomPatterns("foo Location area code: 'A1B2' bar"));

  EXPECT_EQ("foo\na SSID='(SSID: 1)' b\n'",
            RedactCustomPatterns("foo\na SSID='Joe's' b\n'"));
  EXPECT_EQ("ssid '(SSID: 2)'", RedactCustomPatterns("ssid 'My AP'"));
  EXPECT_EQ("bssid 'aa:bb'", RedactCustomPatterns("bssid 'aa:bb'"));

  EXPECT_EQ("Scan SSID - hexdump(len=6): (SSIDHex: 1)\nfoo",
            RedactCustomPatterns(
                "Scan SSID - hexdump(len=6): 47 6f 6f 67 6c 65\nfoo"));

  EXPECT_EQ(
      "a\nb [SSID=(SSID: 3)] [SSID=(SSID: 1)] [SSID=foo\nbar] b",
      RedactCustomPatterns("a\nb [SSID=foo] [SSID=Joe's] [SSID=foo\nbar] b"));
  EXPECT_EQ("ssid=\"(SSID: 4)\"",
            RedactCustomPatterns("ssid=\"LittleTsunami\""));
  EXPECT_EQ("* SSID=(SSID: 5)", RedactCustomPatterns("* SSID=agnagna"));

  EXPECT_EQ("Specifier: (ArcNetworkFactory#1) SSID: \"(SSID: 6)\" foo",
            RedactCustomPatterns(
                "Specifier: (ArcNetworkFactory#1) SSID: \"GoogleGuest1\" foo"));
  EXPECT_EQ("Specifier: (ArcNetworkFactory#1) SSID: '(SSID: 7)' foo",
            RedactCustomPatterns(
                "Specifier: (ArcNetworkFactory#1) SSID: 'GoogleGuest2' foo"));
  EXPECT_EQ("Specifier: (ArcNetworkFactory#1) SSID: (SSID: 8)",
            RedactCustomPatterns(
                "Specifier: (ArcNetworkFactory#1) SSID: GoogleGuest3"));
  EXPECT_EQ(
      "Specifier: (ArcNetworkFactory#1) SSID: (SSID: 9)",
      RedactCustomPatterns(
          "Specifier: (ArcNetworkFactory#1) SSID: less than 32 characters"));
  EXPECT_EQ("Specifier: (ArcNetworkFactory#1) SSID: (SSID: 10)foo",
            RedactCustomPatterns("Specifier: (ArcNetworkFactory#1) SSID: this "
                                 "line is 32 characters long!foo"));
  EXPECT_EQ(
      "<WifiNetworkSpecifier [, SSID Match pattern=PatternMatcher{LITERAL: "
      "(SSID: 11)}, ...]",
      RedactCustomPatterns("<WifiNetworkSpecifier [, SSID Match "
                           "pattern=PatternMatcher{LITERAL: Google-A}, ...]"));

  EXPECT_EQ("SerialNumber: (Serial: 1)",
            RedactCustomPatterns("SerialNumber: 1217D7EF"));
  EXPECT_EQ("serial  number: (Serial: 2)",
            RedactCustomPatterns("serial  number: 50C971FEE7F3x010900"));
  EXPECT_EQ("SerialNumber: (Serial: 3)",
            RedactCustomPatterns("SerialNumber: EVT23-17BA01-004"));
  EXPECT_EQ("serial=\"(Serial: 4)\"",
            RedactCustomPatterns("serial=\"1234AA5678\""));
  EXPECT_EQ("\"serial_number\"=\"(Serial: 1)\"",
            RedactCustomPatterns("\"serial_number\"=\"1217D7EF\""));
  EXPECT_EQ("SerialNumber: (Serial: 5)",
            RedactCustomPatterns("SerialNumber: 5:00:14.0"));
  EXPECT_EQ("Serial: (Serial: 6)",
            RedactCustomPatterns("Serial: ABCEFG\x01kjmn-as:342/234\\432"));
  // Don't overly redact serial numbers, we only do this for a specific
  // formatting case for edid-decode.
  EXPECT_EQ("Foo serial number 123",
            RedactCustomPatterns("Foo serial number 123"));
  EXPECT_EQ("Foo Serial Number (Serial: 7)",
            RedactCustomPatterns("Foo Serial Number 123"));
  // redact serial number separated by a | with the label "serial"
  EXPECT_EQ("serial               | (Serial: 8)",
            RedactCustomPatterns("serial               | 0x1cc04416"));
  EXPECT_EQ("serial               |(Serial: 9)",
            RedactCustomPatterns("serial               |0x1cc04417"));
  EXPECT_EQ("serial|(Serial: 10)", RedactCustomPatterns("serial|0x1cc04418"));
  EXPECT_EQ("serial|(Serial: 11)", RedactCustomPatterns("serial|agnagna"));
  // redact attested device id that is also a serial number
  EXPECT_EQ("\"attested_device_id\"=\"(Serial: 12)\"",
            RedactCustomPatterns("\"attested_device_id\"=\"5CD045B0DZ\""));
  EXPECT_EQ("\"attested_device_id\"=\"(Serial: 13)\"",
            RedactCustomPatterns("\"attested_device_id\"=\"5CD04-5B0DZ\""));
  // The dash cannot appear first or last.
  EXPECT_EQ("\"attested_device_id\"=\"-5CD045B0DZ\"",
            RedactCustomPatterns("\"attested_device_id\"=\"-5CD045B0DZ\""));
  EXPECT_EQ("\"attested_device_id\"=\"5CD045B0DZ-\"",
            RedactCustomPatterns("\"attested_device_id\"=\"5CD045B0DZ-\""));
  // redact lsusb's iSerial with a nonzero index.
  EXPECT_EQ("iSerial    3 (Serial: 14)",
            RedactCustomPatterns("iSerial    3 12345abcdEFG"));
  // Do not redact lsusb's iSerial when the index is 0.
  EXPECT_EQ("iSerial    0 ", RedactCustomPatterns("iSerial    0 "));
  // redact usbguard's serial number in syslog
  EXPECT_EQ("serial \"(Serial: 15)\"",
            RedactCustomPatterns("serial \"usb1234AA5678\""));
  EXPECT_EQ("SN: (Serial: 16)",
            RedactCustomPatterns("SN: ffffffff ffffffff ffffffff"));
  EXPECT_EQ("DEV_ID:      (Serial: 17)",
            RedactCustomPatterns("DEV_ID:      0x1202204d 0x4c29b022"));

  // Valid PSM identifiers.
  EXPECT_EQ("PSM id: (PSM ID: 1)", RedactCustomPatterns("PSM id: ABCZ/123xx"));
  EXPECT_EQ("psm: (PSM ID: 2)", RedactCustomPatterns("psm: ABC123F2/123xx"));
  EXPECT_EQ("PsM: (PSM ID: 3)", RedactCustomPatterns("PsM: abcf6677/123xx"));
  EXPECT_EQ("PSM determination successful. Identifier (PSM ID: 4) not present.",
            RedactCustomPatterns("PSM determination successful. Identifier "
                                 "JTFE/223PE6015195 not present."));
  // Wrong number of brand code characters.
  EXPECT_EQ("PSM: ABC/123xx", RedactCustomPatterns("PSM: ABC/123xx"));
  // Non-hex brand code.
  EXPECT_EQ("PSM: zefg0000/123xx", RedactCustomPatterns("PSM: zefg0000/123xx"));
  // No mention of PSM prior to identifier, e.g. in unrelated paths.
  EXPECT_EQ("/root/123xx", RedactCustomPatterns("/root/123xx"));
  // PSM mention without whitespace, e.g. in base64-encoded data.
  EXPECT_EQ("PSM+ABCZ/123xx", RedactCustomPatterns("PSM+ABCZ/123xx"));
  // PSM mention with only newline whitespace, e.g. in base64-encoded data.
  EXPECT_EQ("PSM+\r\nABCZ/123xx", RedactCustomPatterns("PSM+\r\nABCZ/123xx"));

  EXPECT_EQ("\"gaia_id\":\"(GAIA: 1)\"",
            RedactCustomPatterns("\"gaia_id\":\"1234567890\""));
  EXPECT_EQ("gaia_id='(GAIA: 2)'", RedactCustomPatterns("gaia_id='987654321'"));
  EXPECT_EQ("{id: (GAIA: 1), email:",
            RedactCustomPatterns("{id: 1234567890, email:"));
  EXPECT_EQ("\"accountId\": \"(GAIA: 3)\"",
            RedactCustomPatterns("\"accountId\": \"01234\""));
  EXPECT_EQ("\"label\": \"Account Id\",\n  \"status\": \"(GAIA: 3)\"",
            RedactCustomPatterns(
                "\"label\": \"Account Id\",\n  \"status\": \"01234\""));
  EXPECT_EQ(
      "\"label\": \"Gaia Id\",\n  \"status\": \"(GAIA: 3)\"",
      RedactCustomPatterns("\"label\": \"Gaia Id\",\n  \"status\": \"01234\""));

  EXPECT_EQ("(email: 1)", RedactCustomPatterns("foo@bar.com"));
  EXPECT_EQ("Email: (email: 1).", RedactCustomPatterns("Email: foo@bar.com."));
  EXPECT_EQ("Email:\n(email: 2)\n",
            RedactCustomPatterns("Email:\nfooooo@bar.com\n"));

  EXPECT_EQ("[(IPv6: 1)]",
            RedactCustomPatterns("[2001:0db8:0000:0000:0000:ff00:0042:8329]"));
  EXPECT_EQ("[(IPv6: 2)]",
            RedactCustomPatterns("[2001:db8:0:0:0:ff00:42:8329]"));
  EXPECT_EQ("[(IPv6: 3)]", RedactCustomPatterns("[2001:db8::ff00:42:8329]"));
  EXPECT_EQ("[(IPv6: 4)]", RedactCustomPatterns("[aa::bb]"));
  EXPECT_EQ("State::Abort", RedactCustomPatterns("State::Abort"));

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

  EXPECT_EQ("(URL: 1)", RedactCustomPatterns("http://example.com/foo?test=1"));
  EXPECT_EQ("Foo (URL: 2) Bar",
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
        base::StartsWith(got, "(URL: ", base::CompareCase::INSENSITIVE_ASCII));
    EXPECT_TRUE(base::EndsWith(got, ")", base::CompareCase::INSENSITIVE_ASCII));
  }
  // Test that "Android:" is not considered a schema with empty hier part.
  EXPECT_EQ("The following applies to Android:",
            RedactCustomPatterns("The following applies to Android:"));

  EXPECT_EQ(
      "[  513980.417] (Memory Dump: 1)",
      RedactCustomPatterns(
          "[  513980.417] 0x00005010: 00aaa423 00baa623 00caa823 00daaa23"));
  EXPECT_EQ("[  513980] 0x00005010: 00aaa423 00baa623 00caa823 00daaa23",
            RedactCustomPatterns(
                "[  513980] 0x00005010: 00aaa423 00baa623 00caa823 00daaa23"));
  EXPECT_EQ("[abcdefg] 0x00005010: 00aaa423 00baa623 00caa823 00daaa23",
            RedactCustomPatterns(
                "[abcdefg] 0x00005010: 00aaa423 00baa623 00caa823 00daaa23"));
}

TEST_F(RedactionToolTest, RedactCustomPatternWithContext) {
  // The PIIType for the CustomPatternWithAlias is not relevant, only for
  // testing.
  const CustomPatternWithAlias kPattern1 = {"ID", "(\\b(?i)id:? ')(\\d+)(')",
                                            PIIType::kStableIdentifier};
  const CustomPatternWithAlias kPattern2 = {"ID", "(\\b(?i)id=')(\\d+)(')",
                                            PIIType::kStableIdentifier};
  const CustomPatternWithAlias kPattern3 = {"IDG", "(\\b(?i)idg=')(\\d+)(')",
                                            PIIType::kCellularLocationInfo};
  EXPECT_EQ("", RedactCustomPatternWithContext("", kPattern1));
  EXPECT_EQ("foo\nbar\n",
            RedactCustomPatternWithContext("foo\nbar\n", kPattern1));
  EXPECT_EQ("id '(ID: 1)'",
            RedactCustomPatternWithContext("id '2345'", kPattern1));
  EXPECT_EQ("id '(ID: 2)'",
            RedactCustomPatternWithContext("id '1234'", kPattern1));
  EXPECT_EQ("id: '(ID: 2)'",
            RedactCustomPatternWithContext("id: '1234'", kPattern1));
  EXPECT_EQ("ID: '(ID: 1)'",
            RedactCustomPatternWithContext("ID: '2345'", kPattern1));
  EXPECT_EQ("x1 id '(ID: 1)' 1x id '(ID: 2)'\nid '(ID: 1)'\n",
            RedactCustomPatternWithContext(
                "x1 id '2345' 1x id '1234'\nid '2345'\n", kPattern1));
  // Different pattern with same alias should reuse the replacements.
  EXPECT_EQ("id='(ID: 2)'",
            RedactCustomPatternWithContext("id='1234'", kPattern2));
  // Different alias should not reuse replacement from another pattern.
  EXPECT_EQ("idg='(IDG: 1)'",
            RedactCustomPatternWithContext("idg='1234'", kPattern3));
  EXPECT_EQ("x(FOO: 1)z",
            RedactCustomPatternWithContext("xyz", {"FOO", "()(y+)()"}));

  // Real IPv4 address
  EXPECT_EQ("(IPv4: 1)", RedactCustomPatterns("192.160.0.1"));
  EXPECT_EQ("[(IPv4: 1)]", RedactCustomPatterns("[192.160.0.1]"));
  EXPECT_EQ("aaaa(IPv4: 2)aaa", RedactCustomPatterns("aaaa123.123.45.4aaa"));
  EXPECT_EQ("IP: (IPv4: 3)", RedactCustomPatterns("IP: 111.222.3.4"));
  EXPECT_EQ("(email: 1) (IPv4: 3)",
            RedactCustomPatterns("test@email.com 111.222.3.4"));
  EXPECT_EQ(
      "(URL: 1) (email: 1) (IPv4: 3)",
      RedactCustomPatterns("http://www.google.com test@email.com 111.222.3.4"));
  EXPECT_EQ("addresses=(IPv4: 4)/30,x",
            RedactCustomPatterns("addresses=100.100.1.10/30,x"));

  // Non-PII IPv4 address (see MaybeScrubIPAddress)
  EXPECT_EQ("255.255.255.255", RedactCustomPatterns("255.255.255.255"));

  // Not an actual IPv4 address
  EXPECT_EQ("75.748.86.91", RedactCustomPatterns("75.748.86.91"));
  EXPECT_EQ("1.2.3.4.5", RedactCustomPatterns("1.2.3.4.5"));
  EXPECT_EQ("1.2.3.4.5.6.7.8", RedactCustomPatterns("1.2.3.4.5.6.7.8"));

  // USB Path - not an actual IPv4 Address
  EXPECT_EQ("4-3.3.3.3", RedactCustomPatterns("4-3.3.3.3"));
}

TEST_F(RedactionToolTest, RedactCustomPatternWithoutContext) {
  // The PIIType for the CustomPatternWithAlias here is not relevant, only for
  // testing.
  CustomPatternWithAlias kPattern = {"pattern", "(o+)", PIIType::kEmail};
  EXPECT_EQ("", RedactCustomPatternWithoutContext("", kPattern));
  EXPECT_EQ("f(pattern: 1)\nf(pattern: 2)z\nf(pattern: 1)l\n",
            RedactCustomPatternWithoutContext("fo\nfooz\nfol\n", kPattern));
}

TEST_F(RedactionToolTest, RedactChunk) {
  redactor_.EnableCreditCardRedaction(true);
  std::string redaction_input;
  std::string redaction_output;
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kUnitTest, 0);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kSysLogUploader, 0);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kSysLogFetcher, 0);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kSupportTool, 0);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kErrorReporting, 0);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kFeedbackToolHotRod, 0);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kFeedbackToolUserDescriptions, 0);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kFeedbackToolLogs, 0);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kCrashTool, 0);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kCrashToolJSErrors, 0);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kUndetermined, 0);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kUnknown, 0);

  using enum CreditCardDetection;
  ExpectBucketCount(kCreditCardRedactionHistogram, kRegexMatch, 0);
  ExpectBucketCount(kCreditCardRedactionHistogram, kTimestamp, 0);
  ExpectBucketCount(kCreditCardRedactionHistogram, kRepeatedChars, 0);
  ExpectBucketCount(kCreditCardRedactionHistogram, kDoesntValidate, 0);
  ExpectBucketCount(kCreditCardRedactionHistogram, kValidated, 0);
  EXPECT_EQ(metrics_tester_->GetNumBucketEntries(
                RedactionToolMetricsRecorder::
                    GetTimeSpentRedactingHistogramNameForTesting()),
            0u);

  for (int enum_int = static_cast<int>(PIIType::kNone) + 1;
       enum_int <= static_cast<int>(PIIType::kMaxValue); ++enum_int) {
    const PIIType enum_value = static_cast<PIIType>(enum_int);
    ExpectBucketCount(kPIIRedactedHistogram, enum_value, 0);
  }

  for (const auto& s : kStringsWithRedactions) {
    redaction_input.append(s.pre_redaction).append("\n");
    redaction_output.append(s.post_redaction).append("\n");
  }
  EXPECT_EQ(redaction_output, redactor_.Redact(redaction_input));

  for (int enum_int = static_cast<int>(PIIType::kNone) + 1;
       enum_int <= static_cast<int>(PIIType::kMaxValue); ++enum_int) {
    const PIIType enum_value = static_cast<PIIType>(enum_int);
    const size_t expected_count = base::ranges::count_if(
        kStringsWithRedactions,
        [enum_value](const StringWithRedaction& string_with_redaction) {
          return string_with_redaction.pii_type == enum_value;
        });
    ExpectBucketCount(kPIIRedactedHistogram, enum_value, expected_count);
  }
  // This isn't handled by the redaction tool but rather in the
  // `UiHierarchyDataCollector`. It's part of the enum for historical reasons.
  ExpectBucketCount(kPIIRedactedHistogram, PIIType::kUIHierarchyWindowTitles,
                    0);
  // This isn't handled by the redaction tool but rather in Shill. It's part of
  // the enum for historical reasons.
  ExpectBucketCount(kPIIRedactedHistogram, PIIType::kEAP, 0);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kUnitTest, 1);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kSysLogUploader, 0);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kSysLogFetcher, 0);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kSupportTool, 0);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kErrorReporting, 0);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kFeedbackToolHotRod, 0);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kFeedbackToolUserDescriptions, 0);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kFeedbackToolLogs, 0);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kCrashTool, 0);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kCrashToolJSErrors, 0);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kUndetermined, 0);
  ExpectBucketCount(kRedactionToolCallerHistogram,
                    RedactionToolCaller::kUnknown, 0);

  ExpectBucketCount(kCreditCardRedactionHistogram, kRegexMatch, 16);
  ExpectBucketCount(kCreditCardRedactionHistogram, kTimestamp, 2);
  ExpectBucketCount(kCreditCardRedactionHistogram, kRepeatedChars, 1);
  ExpectBucketCount(kCreditCardRedactionHistogram, kDoesntValidate, 8);
  ExpectBucketCount(kCreditCardRedactionHistogram, kValidated, 5);
  EXPECT_EQ(metrics_tester_->GetNumBucketEntries(
                RedactionToolMetricsRecorder::
                    GetTimeSpentRedactingHistogramNameForTesting()),
            1u);
}

TEST_F(RedactionToolTest, RedactAndKeepSelected) {
  redactor_.EnableCreditCardRedaction(true);
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
  EXPECT_EQ("UID: (UID: 1)",
            redactor_.RedactAndKeepSelected(
                "UID: B3mcFTkQAHofv94DDTUuVJGGEI/BbzsyDncplMCR2P4=", {}));
  // base64-encoded 33 bytes should not be treated as UID.
  EXPECT_EQ("MDEyMzQ1Njc4OTAxMjM0NTY3ODkwMTIzNDU2Nzg5MDEyCg==",
            redactor_.RedactAndKeepSelected(
                "MDEyMzQ1Njc4OTAxMjM0NTY3ODkwMTIzNDU2Nzg5MDEyCg==", {}));
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
     "chrome://resources/f?user=(HASH:9988 1)"},  // URL that contains a hash.
    {"/root/27540283740a0897ab7c8de0f809add2bacde78f/foo",
     "/root/(HASH:2754 2)/foo"},  // String that contains a hash.
    {"this is the user hash that we need to redact "
     "aabbccddeeff00112233445566778899",
     "this is the user hash that we need to redact (HASH:aabb 3)"},  // String
                                                                     // that
                                                                     // contains
                                                                     // a hash.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"8.0K\t/home/root/aabbccddeeff00112233445566778899/"
     "android-data/data/data/pa.ckage2/de",  // Android app storage
                                             // path that contains a
                                             // hash.
     "8.0K\t/home/root/(HASH:aabb 3)/android-data/data/data/pa.ckage2/de"}
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
        {PIIType::kMACAddress, {"aa:aa:aa:aa:aa:aa"}},
        {PIIType::kStableIdentifier,
         {
             "27540283740a0897ab7c8de0f809add2bacde78f",
             "B3mcFTkQAHofv94DDTUuVJGGEI/BbzsyDncplMCR2P4=",
         }},
        {PIIType::kCreditCard,
         {"4012888888881881", "5019717010103742", "5019717010103742787"}},
        {PIIType::kIBAN, {"GB82WEST12345698765432", "GB33BUKB20201555555555"}},
    {
      PIIType::kCrashId, {
        "153c963587d8d8d4", "153C963587D8D8D4b"
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

#if !BUILDFLAG(IS_IOS)
// TODO(xiangdongkong): Make the test work on IOS builds. Current issue: the
// test files do not exist.
//
// Redact the text in the input file "test_data/test_logs.txt".
// The expected output is from "test_data/test_logs_redacted.txt".
TEST_F(RedactionToolTest, RedactTextFileContent) {
  base::FilePath base_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &base_path);
  base_path = base_path.AppendASCII("components/feedback/redaction_tool")
                  .AppendASCII("test_data");

  std::string text_to_be_redacted;
  std::string text_redacted;
  ASSERT_TRUE(base::ReadFileToString(base_path.AppendASCII("test_logs.txt"),
                                     &text_to_be_redacted));
  ASSERT_TRUE(base::ReadFileToString(
      base_path.AppendASCII("test_logs_redacted.txt"), &text_redacted));

  EXPECT_EQ(text_redacted, redactor_.Redact(text_to_be_redacted));
}

#endif  // !BUILDFLAG(IS_IOS)

TEST_F(RedactionToolTest, RedactBlockDevices) {
  // Test cases in the form {input, output}.
  std::pair<std::string, std::string> test_cases[] = {
      // UUIDs that come from the 'blkid' tool.
      {"PTUUID=\"985dff64-9c0f-3f49-945b-2d8c2e0238ec\"",
       "PTUUID=\"(UUID: 1)\""},
      {"UUID=\"E064-868C\"", "UUID=\"(UUID: 2)\""},
      {"PARTUUID=\"7D242B2B1C751832\"", "PARTUUID=\"(UUID: 3)\""},

      // Volume labels.
      {"LABEL=\"ntfs\"", "LABEL=\"(Volume Label: 1)\""},
      {"PARTLABEL=\"SD Card\"", "PARTLABEL=\"(Volume Label: 2)\""},

      // LVM UUIDd.
      {"{\"pv_fmt\":\"lvm2\", "
       "\"pv_uuid\":\"duD18x-P7QE-sTya-SaeO-aq07-YgEq-xj8UEz\", "
       "\"dev_size\":\"230.33g\"}",
       "{\"pv_fmt\":\"lvm2\", \"pv_uuid\":\"(UUID: 4)\", "
       "\"dev_size\":\"230.33g\"}"},
      {"{\"lv_uuid\":\"lKYORl-TWDP-OFLT-yDnB-jlQ7-aQrE-AwA8Oa\", "
       "\"lv_name\":\"[thinpool_tdata]\"",
       "{\"lv_uuid\":\"(UUID: 5)\", \"lv_name\":\"[thinpool_tdata]\""},
      {"id = \"KJ0bUk-QE15-mNMp-6Z2V-4Efq-N1r4-oPeFyc\"", "id = \"(UUID: 6)\""},

      // Removable media paths.
      {"/media/removable/SD Card/", "/media/removable/(Volume Label: 2)/"},
      {"'/media/removable/My Secret Volume Name' don't redact this",
       "'/media/removable/(Volume Label: 3)' don't redact this"},
      {"0 part /media/removable/My Secret Volume Name         With Spaces   ",
       "0 part /media/removable/(Volume Label: 4)"},
  };
  for (const auto& p : test_cases) {
    EXPECT_EQ(redactor_.Redact(p.first), p.second);
  }
}

}  // namespace redaction
