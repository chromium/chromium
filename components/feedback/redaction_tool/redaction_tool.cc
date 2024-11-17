// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/feedback/redaction_tool/redaction_tool.h"

#include <algorithm>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "build/chromeos_buildflags.h"
#include "components/autofill/core/common/credit_card_number_validation.h"
#include "components/feedback/redaction_tool/ip_address.h"
#include "components/feedback/redaction_tool/pii_types.h"
#ifdef USE_SYSTEM_RE2
#include <re2/re2.h>
#else
#include "third_party/re2/src/re2/re2.h"
#endif  // USE_SYSTEM_RE2

using re2::RE2;
using redaction_internal::IPAddress;

namespace redaction {

namespace features {
COMPONENT_EXPORT(REDACTION_TOOL)
BASE_FEATURE(kEnableCreditCardRedaction,
             "EnableCreditCardRedaction",
             base::FEATURE_ENABLED_BY_DEFAULT);

COMPONENT_EXPORT(REDACTION_TOOL)
BASE_FEATURE(kEnableIbanRedaction,
             "EnableIbanRedaction",
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace features

namespace {

// Helper macro: Non capturing group
#define NCG(x) "(?:" x ")"
// Helper macro: Optional non capturing group
#define OPT_NCG(x) NCG(x) "?"

//////////////////////////////////////////////////////////////////////////
// Patterns for URLs, or better IRIs, based on RFC 3987 with an artificial
// limitation on the scheme to increase precision. Otherwise anything
// like "ID:" would be considered an IRI.

#define UNRESERVED "[-a-z0-9._~]"
#define RESERVED NGC(GEN_DELIMS "|" SUB_DELIMS)
#define SUB_DELIMS "[!$&'()*+,;=]"
#define GEN_DELIMS "[:/?#[\\]@]"

#define DIGIT "[0-9]"
#define HEXDIG "[0-9a-f]"

#define PCT_ENCODED "%" HEXDIG HEXDIG

#define DEC_OCTET NCG("1[0-9][0-9]|2[0-4][0-9]|25[0-5]|[1-9][0-9]|[0-9]")

#define IPV4ADDRESS DEC_OCTET "\\." DEC_OCTET "\\." DEC_OCTET "\\." DEC_OCTET

#define H16 NCG(HEXDIG) "{1,4}"
#define LS32 NCG(H16 ":" H16 "|" IPV4ADDRESS)
#define WB "\\b"

// clang-format off
#define IPV6ADDRESS NCG( \
                                          WB NCG(H16 ":") "{6}" LS32 WB "|" \
                                        "::" NCG(H16 ":") "{5}" LS32 WB "|" \
  OPT_NCG( WB                      H16) "::" NCG(H16 ":") "{4}" LS32 WB "|" \
  OPT_NCG( WB NCG(H16 ":") "{0,1}" H16) "::" NCG(H16 ":") "{3}" LS32 WB "|" \
  OPT_NCG( WB NCG(H16 ":") "{0,2}" H16) "::" NCG(H16 ":") "{2}" LS32 WB "|" \
  OPT_NCG( WB NCG(H16 ":") "{0,3}" H16) "::" NCG(H16 ":")       LS32 WB "|" \
  OPT_NCG( WB NCG(H16 ":") "{0,4}" H16) "::"                    LS32 WB "|" \
  OPT_NCG( WB NCG(H16 ":") "{0,5}" H16) "::"                    H16  WB "|" \
  OPT_NCG( WB NCG(H16 ":") "{0,6}" H16) "::")
// clang-format on

#define IPVFUTURE                     \
  "v" HEXDIG                          \
  "+"                                 \
  "\\." NCG(UNRESERVED "|" SUB_DELIMS \
                       "|"            \
                       ":") "+"

#define IP_LITERAL "\\[" NCG(IPV6ADDRESS "|" IPVFUTURE) "\\]"

#define PORT DIGIT "*"

// This is a diversion of RFC 3987
#define SCHEME \
  NCG("http|https|ftp|chrome|chrome-extension|android|rtsp|file|isolated-app")

#define IPRIVATE            \
  "["                       \
  "\\x{E000}-\\x{F8FF}"     \
  "\\x{F0000}-\\x{FFFFD}"   \
  "\\x{100000}-\\x{10FFFD}" \
  "]"

#define UCSCHAR           \
  "["                     \
  "\\x{A0}-\\x{D7FF}"     \
  "\\x{F900}-\\x{FDCF}"   \
  "\\x{FDF0}-\\x{FFEF}"   \
  "\\x{10000}-\\x{1FFFD}" \
  "\\x{20000}-\\x{2FFFD}" \
  "\\x{30000}-\\x{3FFFD}" \
  "\\x{40000}-\\x{4FFFD}" \
  "\\x{50000}-\\x{5FFFD}" \
  "\\x{60000}-\\x{6FFFD}" \
  "\\x{70000}-\\x{7FFFD}" \
  "\\x{80000}-\\x{8FFFD}" \
  "\\x{90000}-\\x{9FFFD}" \
  "\\x{A0000}-\\x{AFFFD}" \
  "\\x{B0000}-\\x{BFFFD}" \
  "\\x{C0000}-\\x{CFFFD}" \
  "\\x{D0000}-\\x{DFFFD}" \
  "\\x{E1000}-\\x{EFFFD}" \
  "]"

#define IUNRESERVED  \
  NCG("[-a-z0-9._~]" \
      "|" UCSCHAR)

#define IPCHAR                                   \
  NCG(IUNRESERVED "|" PCT_ENCODED "|" SUB_DELIMS \
                  "|"                            \
                  "[:@]")
#define IFRAGMENT \
  NCG(IPCHAR      \
      "|"         \
      "[/?]")     \
  "*"
#define IQUERY            \
  NCG(IPCHAR "|" IPRIVATE \
             "|"          \
             "[/?]")      \
  "*"

#define ISEGMENT IPCHAR "*"
#define ISEGMENT_NZ IPCHAR "+"
#define ISEGMENT_NZ_NC                           \
  NCG(IUNRESERVED "|" PCT_ENCODED "|" SUB_DELIMS \
                  "|"                            \
                  "@")                           \
  "+"

#define IPATH_EMPTY ""
#define IPATH_ROOTLESS ISEGMENT_NZ NCG("/" ISEGMENT) "*"
#define IPATH_NOSCHEME ISEGMENT_NZ_NC NCG("/" ISEGMENT) "*"
#define IPATH_ABSOLUTE "/" OPT_NCG(ISEGMENT_NZ NCG("/" ISEGMENT) "*")
#define IPATH_ABEMPTY NCG("/" ISEGMENT) "*"

#define IPATH                                                                \
  NCG(IPATH_ABEMPTY "|" IPATH_ABSOLUTE "|" IPATH_NOSCHEME "|" IPATH_ROOTLESS \
                    "|" IPATH_EMPTY)

#define IREG_NAME NCG(IUNRESERVED "|" PCT_ENCODED "|" SUB_DELIMS) "*"

#define IHOST NCG(IP_LITERAL "|" IPV4ADDRESS "|" IREG_NAME)
#define IUSERINFO                                \
  NCG(IUNRESERVED "|" PCT_ENCODED "|" SUB_DELIMS \
                  "|"                            \
                  ":")                           \
  "*"
#define IAUTHORITY OPT_NCG(IUSERINFO "@") IHOST OPT_NCG(":" PORT)

#define IRELATIVE_PART                                                    \
  "//" NCG(IAUTHORITY IPATH_ABEMPTY "|" IPATH_ABSOLUTE "|" IPATH_NOSCHEME \
                                    "|" IPATH_EMPTY)

#define IRELATIVE_REF IRELATIVE_PART OPT_NCG("?" IQUERY) OPT_NCG("#" IFRAGMENT)

// RFC 3987 requires IPATH_EMPTY here but it is omitted so that statements
// that end with "Android:" for example are not considered a URL.
#define IHIER_PART \
  "//" NCG(IAUTHORITY IPATH_ABEMPTY "|" IPATH_ABSOLUTE "|" IPATH_ROOTLESS)

#define ABSOLUTE_IRI SCHEME ":" IHIER_PART OPT_NCG("?" IQUERY)

#define IRI SCHEME ":" IHIER_PART OPT_NCG("\\?" IQUERY) OPT_NCG("#" IFRAGMENT)

#define IRI_REFERENCE NCG(IRI "|" IRELATIVE_REF)

// The |kCustomPatternsWithContext| array defines patterns to match and
// redact. Each pattern needs to define three capturing parentheses groups:
//
// - a group for the pattern before the identifier to be redacted;
// - a group for the identifier to be redacted;
// - a group for the pattern after the identifier to be redacted.
//
// The first and the last capture group are the origin of the "WithContext"
// suffix in the name of this constant.
//
// Every matched identifier (in the context of the whole pattern) is redacted
// by replacing it with an incremental instance identifier. Every different
// pattern defines a separate instance identifier space. See the unit test for
// RedactionToolTest::RedactCustomPatterns for pattern redaction examples.
//
// Useful regular expression syntax:
//
// +? is a non-greedy (lazy) +.
// \b matches a word boundary.
// (?i) turns on case insensitivity for the remainder of the regex.
// (?-s) turns off "dot matches newline" for the remainder of the regex.
// (?:regex) denotes non-capturing parentheses group.
CustomPatternWithAlias kCustomPatternsWithContext[] = {
    // ModemManager
    {"CellID", "(\\bCell ID: ')([0-9a-fA-F]+)(')",
     PIIType::kCellularLocationInfo},
    {"LocAC", "(\\bLocation area code: ')([0-9a-fA-F]+)(')",
     PIIType::kCellularLocationInfo},

    // Android. Must run first since this expression matches the replacement.
    //
    // If we don't get helpful delimiters like a single/double quote, then we
    // can only try our best and take out the next 32 characters, the max length
    // of a SSID. Require at least one non-quote character though so we skip
    // over the quoted SSIDs (which the following patterns will catch and
    // redact).
    {"SSID", "(?i-s)(\\bSSID: )([^'\"]{1,32})(.*)", PIIType::kSSID},
    // Replace any SSID inside quotes.
    {"SSID", "(?i-s)(\\bSSID: ['\"])(.+)(['\"])", PIIType::kSSID},
    // Special WifiNetworkSpecifier#toString.
    {"SSID", "(?i-s)(\\bSSID Match pattern=[^ ]*\\s?)(.+)(\\})",
     PIIType::kSSID},

    // wpa_supplicant
    {"SSID", "(?i-s)(\\bssid[= ]')(.+)(')", PIIType::kSSID},
    {"SSID", "(?i-s)(\\bssid[= ]\")(.+)(\")", PIIType::kSSID},
    {"SSID", "(\\* SSID=)([^\n]+)(.*)", PIIType::kSSID},
    {"SSIDHex", "(?-s)(\\bSSID - hexdump\\(len=[0-9]+\\): )(.+)()",
     PIIType::kSSID},

    // shill
    {"SSID", "(?-s)(\\[SSID=)(.+?)(\\])", PIIType::kSSID},

    // Serial numbers. The actual serial number itself can include any alphanum
    // char as well as dashes, periods, colons, slashes and unprintable ASCII
    // chars (except newline). The second one is for a special case in
    // edid-decode, where if we genericized it further then we would catch too
    // many other cases that we don't want to redact.
    {"Serial",
     "(?i-s)(\\bserial\\s*_?(?:number)?['\"]?\\s*[:=|]\\s*['\"]?)"
     "([0-9a-zA-Z\\-.:\\/\\\\\\x00-\\x09\\x0B-\\x1F]+)(\\b)",
     PIIType::kSerial},
    {"Serial", "( Serial Number )(\\d+)(\\b)", PIIType::kSerial},
    // USB Serial numbers, as outputted from the lsusb --verbose tool.
    // "iSerial" followed by some spaces, then up to 5 digits of the iSerial
    // index which is not part of the serial number itself, followed by the
    // serial number string.
    // The iSerial index must be nonzero, as an index of zero indicates no
    // string descriptor is present.
    // The serial number string itself is up to the manufacturer, but is
    // observed to be alphanumetric (numbers, and both upper and lower case
    // letters).
    {"Serial", "(iSerial\\s*[1-9]\\d{0,4}\\s)([0-9a-zA-Z-]+)(\\b)",
     PIIType::kSerial},
    // USB Serial number as generated by usbguard.
    {"Serial",
     "(?i-s)(\\bserial\\s\")"
     "([0-9a-z\\-.:\\/\\\\\\x00-\\x09\\x0B-\\x1F]+)(\")",
     PIIType::kSerial},
    // The attested device id, a serial number, that comes from vpd_2.0.txt.
    // The pattern was recently clarified as being a case insensitive string of
    // ASCII letters and digits, plus the dash/hyphen character. The dash cannot
    // appear first or last
    {"Serial", "(\"attested_device_id\"=\")([^-][0-9a-zA-Z-]+[^-])(\")",
     PIIType::kSerial},
    // PSM identifier is a 4-character brand code, which can be encoded as 8 hex
    // digits, followed by a slash ('/') and a serial number.
    {"PSM ID",
     "(?i)(PSM.*[\t ]+.*\\b)((?:[a-z]{4}|[0-9a-f]{8})\\/"
     "[0-9a-z\\-.:\\/\\\\\\x00-\\x09\\x0B-\\x1F]+)(\\b)",
     PIIType::kSerial},

    // GAIA IDs
    {"GAIA", R"xxx((\"?\bgaia_id\"?[=:]['\"])(\d+)(\b['\"]))xxx",
     PIIType::kGaiaID},
    {"GAIA", R"xxx((\{id: )(\d+)(, email:))xxx", PIIType::kGaiaID},
    // The next two patterns are used by support tool when exporting PII.
    {"GAIA", R"xxx(("accountId":\s*")([^"]+)("))xxx", PIIType::kGaiaID},
    {"GAIA",
     R"xxx(("label":\s*"(?:Account|Gaia) Id",\s*"status":\s*")([^"]+)("))xxx",
     PIIType::kGaiaID},

    // UUIDs given by the 'blkid' tool. These don't necessarily look like
    // standard UUIDs, so treat them specially.
    {"UUID", R"xxx((UUID=")([0-9a-zA-Z-]+)("))xxx", PIIType::kStableIdentifier},
    // Also cover UUIDs given by the 'lvs' and 'pvs' tools, which similarly
    // don't necessarily look like standard UUIDs.
    {"UUID", R"xxx(("[lp]v_uuid":")([0-9a-zA-Z-]+)("))xxx",
     PIIType::kStableIdentifier},
    // Cover UUIDs generated by vgcfgbackup, which also don't look like standard
    // UUIDs.
    {"UUID", R"xxx((id = ")([0-9a-zA-Z-]+)("))xxx", PIIType::kStableIdentifier},

    // Volume labels presented in the 'blkid' tool, and as part of removable
    // media paths shown in various logs such as cros-disks (in syslog).
    // There isn't a well-defined format for these. For labels in blkid,
    // capture everything between the open and closing quote.
    {"Volume Label", R"xxx((LABEL=")([^"]+)("))xxx", PIIType::kVolumeLabel},
    // For paths, this is harder. The only restricted characters are '/' and
    // NUL, so use a simple heuristic. cros-disks generally quotes paths using
    // single-quotes, so capture everything until a quote character. For lsblk,
    // capture everything until the end of the line, since the mount path is the
    // last field.
    {"Volume Label", R"xxx((/media/removable/)(.+?)(['"/\n]|$))xxx",
     PIIType::kVolumeLabel},

    // IPP (Internet Printing Protocol) Addresses
    {"IPP Address", R"xxx((ipp:\/\/)(.+?)(\/ipp))xxx", PIIType::kIPPAddress},
    // Crash ID. This pattern only applies to ChromeOS and it matches the
    // log entries from ChromeOS's crash_sender program.
    {"Crash ID", R"xxx((Crash report receipt ID )([0-9a-fA-F]+)(.+?))xxx",
     PIIType::kCrashId},

    // Names of ChromeOS cryptohome logical volumes and device mapper devices,
    // which include a partial hash of the user id.
    {"UID", R"xxx(((?:cryptohome|dmcrypt)-+)([0-9a-fA-F]+)(-+))xxx",
     PIIType::kStableIdentifier},

    // GSC device id unique to each chip.
    {"Serial",
     R"xxx((DEV_ID:\s+)(0x[0-9a-zA-Z-]{8}\s+0x[0-9a-zA-Z-]{8})(.*?))xxx",
     PIIType::kSerial},

    // Chromebook serial hash stored in GSC.
    {"Serial",
     R"xxx((SN:\s+)([0-9a-zA-Z-]{8}\s+[0-9a-zA-Z-]{8}\s+[0-9a-zA-Z-]{8}))xxx"
     R"xxx((.*?))xxx",
     PIIType::kSerial},

    // Memory dump from GSC log.
    {"Memory Dump",
     R"xxx((\[\s*[0-9]+\.[0-9]+\]\s+)(0x[0-9a-zA-Z-]{8}:\s+[0-9a-zA-Z-]{8})xxx"
     R"xxx(\s+[0-9a-zA-Z-]{8}\s+[0-9a-zA-Z-]{8}\s+[0-9a-zA-Z-]{8})(.*?))xxx",
     PIIType::kMemory},

    // IPv4 addresses should not be prefixed or postfixed by a '.' or a '-'
    // which indicates a version number or other identifier.
    {"IPv4",
     "([^-\\.0-9]|^)"
     "(" IPV4ADDRESS ")"
     "([^-\\.0-9]|$)",
     PIIType::kIPAddress},
};

bool MaybeUnmapAddress(IPAddress* addr) {
  if (!addr->IsIPv4MappedIPv6()) {
    return false;
  }

  *addr = ConvertIPv4MappedIPv6ToIPv4(*addr);
  return true;
}

bool MaybeUntranslateAddress(IPAddress* addr) {
  if (!addr->IsIPv6()) {
    return false;
  }

  static const IPAddress kTranslated6To4(0, 0x64, 0xff, 0x9b, 0, 0, 0, 0, 0, 0,
                                         0, 0, 0, 0, 0, 0);
  if (!IPAddressMatchesPrefix(*addr, kTranslated6To4, 96)) {
    return false;
  }

  const auto bytes = addr->bytes();
  *addr = IPAddress(bytes[12], bytes[13], bytes[14], bytes[15]);
  return true;
}

// If |addr| points to a valid IPv6 address, this function truncates it at /32.
bool MaybeTruncateIPv6(IPAddress* addr) {
  if (!addr->IsIPv6()) {
    return false;
  }

  const auto bytes = addr->bytes();
  *addr = IPAddress(bytes[0], bytes[1], bytes[2], bytes[3], 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0);
  return true;
}

// Returns an appropriately scrubbed version of |addr| if applicable.
std::string MaybeScrubIPAddress(const std::string& addr) {
  struct {
    IPAddress ip_addr;
    int prefix_length;
    bool scrub;
  } static const kNonIdentifyingIPRanges[] = {
      // Private.
      {IPAddress(10, 0, 0, 0), 8, true},
      {IPAddress(172, 16, 0, 0), 12, true},
      {IPAddress(192, 168, 0, 0), 16, true},
      // Chrome OS containers and VMs.
      {IPAddress(100, 115, 92, 0), 24, false},
      // Loopback.
      {IPAddress(127, 0, 0, 0), 8, true},
      // Any.
      {IPAddress(0, 0, 0, 0), 8, true},
      // DNS.
      {IPAddress(8, 8, 8, 8), 32, false},
      {IPAddress(8, 8, 4, 4), 32, false},
      {IPAddress(1, 1, 1, 1), 32, false},
      // Multicast.
      {IPAddress(224, 0, 0, 0), 4, true},
      // Link local.
      {IPAddress(169, 254, 0, 0), 16, true},
      {IPAddress(0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), 10,
       true},
      // Broadcast.
      {IPAddress(255, 255, 255, 255), 32, false},
      // IPv6 loopback, unspecified and non-address strings.
      {IPAddress::IPv6AllZeros(), 112, false},
      // IPv6 multicast all nodes and routers.
      {IPAddress(0xff, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1), 128,
       false},
      {IPAddress(0xff, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2), 128,
       false},
      {IPAddress(0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1), 128,
       false},
      {IPAddress(0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2), 128,
       false},
      // IPv6 other multicast (link and interface local).
      {IPAddress(0xff, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), 16,
       true},
      {IPAddress(0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), 16,
       true},

  };
  IPAddress input_addr;
  if (input_addr.AssignFromIPLiteral(addr) && input_addr.IsValid()) {
    bool mapped = MaybeUnmapAddress(&input_addr);
    bool translated = !mapped ? MaybeUntranslateAddress(&input_addr) : false;
    for (const auto& range : kNonIdentifyingIPRanges) {
      if (IPAddressMatchesPrefix(input_addr, range.ip_addr,
                                 range.prefix_length)) {
        std::string prefix;
        std::string out_addr = addr;
        if (mapped) {
          prefix = "M ";
          out_addr = input_addr.ToString();
        } else if (translated) {
          prefix = "T ";
          out_addr = input_addr.ToString();
        }
        if (range.scrub) {
          out_addr = base::StringPrintf(
              "%s/%d", range.ip_addr.ToString().c_str(), range.prefix_length);
        }
        return base::StrCat({prefix, out_addr});
      }
    }
    // |addr| may have been over-aggressively matched as an IPv6 address when
    // it's really just an arbitrary part of a sentence. If the string is the
    // same as the coarsely truncated address then keep it because even if
    // it happens to be a real address, there is no leak.
    if (MaybeTruncateIPv6(&input_addr) && input_addr.ToString() == addr) {
      return addr;
    }
  }
  return "";
}

// Some strings can contain pieces that match like IPv4 addresses but aren't.
// This function can be used to determine if this was the case by evaluating
// the skipped piece. It returns true, if the matched address was erroneous
// and should be skipped instead.
bool ShouldSkipIPv4Address(std::string_view skipped) {
  // Only look for patterns on the same line as the IPv4 address.
  const auto nlpos = skipped.rfind("\n");
  if (nlpos != std::string_view::npos) {
    skipped = skipped.substr(nlpos);
  }
  // MomdemManager can dump out firmware revision fields that can also
  // confuse the IPv4 matcher e.g. "Revision: 81600.0000.00.29.19.16_DO"
  // so ignore the replacement if the skipped piece looks like
  // "Revision: .*<ipv4>". Note however that if this field contains
  // values delimited by multiple spaces, any matches after the first
  // will lose the context and be redacted.
  static const std::string_view rev("Revision: ");
  static const std::string_view space(" ");
  const auto pos = skipped.rfind(rev);
  if (pos != std::string_view::npos &&
      skipped.find(space, pos + rev.length()) == std::string_view::npos) {
    return true;
  }
  // URLs with an IP Address should be handled by the "URL" entry in
  // kCustomPatternsWithoutContext instead. If the skipped piece ends with an
  // IRI, skip it.
  re2::RE2 re_iri(".*" IRI);
  if (re2::RE2::FullMatch(skipped, re_iri)) {
    return true;
  }
  return false;
}

// TODO(battre): Use http://tools.ietf.org/html/rfc5322 to represent email
// addresses. Capture names as well ("First Lastname" <foo@bar.com>).

// The |kCustomPatternWithoutContext| array defines further patterns to match
// and redact. Each pattern consists of a single capturing group.
CustomPatternWithAlias kCustomPatternsWithoutContext[] = {
    {"URL", "(?i)(" IRI ")", PIIType::kURL},
    // Email Addresses need to come after URLs because they can be part
    // of a query parameter.
    {"email", "(?i)([0-9a-z._%+-]+@[a-z0-9.-]+\\.[a-z]{2,6})", PIIType::kEmail},
    // IPv4 uses context to avoid false positives in version numbers, etc.
    {"IPv6", "(?i)(" IPV6ADDRESS ")", PIIType::kIPAddress},
    // Universal Unique Identifiers (UUIDs).
    {"UUID",
     "(?i)([0-9a-zA-Z]{8}-[0-9a-zA-Z]{4}-[0-9a-zA-Z]{4}-[0-9a-zA-Z]{4}-"
     "[0-9a-zA-Z]{12})",
     PIIType::kStableIdentifier},
    // Eche UID which is a base64 conversion of a 32 bytes public key.
    {"UID",
     "(?:[^A-Za-z0-9+/])"
     "((?:[A-Za-z0-9+/]{4}){10}(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=))",
     PIIType::kStableIdentifier},
};

// Like RE2's FindAndConsume, searches for the first occurrence of |pattern| in
// |input| and consumes the bytes until the end of the pattern matching. Unlike
// FindAndConsume, the bytes skipped before the match of |pattern| are stored
// in |skipped_input|. |args| needs to contain at least one element.
// Returns whether a match was found.
//
// Example: input = "aaabbbc", pattern = "(b+)" leads to skipped_input = "aaa",
// args[0] = "bbb", and the beginning input is moved to the right so that it
// only contains "c".
// Example: input = "aaabbbc", pattern = "(z+)" leads to input = "aaabbbc",
// the args values are not modified and skipped_input is not modified.
bool FindAndConsumeAndGetSkippedN(std::string_view* input,
                                  const re2::RE2& pattern,
                                  std::string_view* skipped_input,
                                  std::string_view* args[],
                                  int argc) {
  std::string_view old_input = *input;

  CHECK_GE(argc, 1);
  re2::RE2::Arg a0(argc > 0 ? args[0] : nullptr);
  re2::RE2::Arg a1(argc > 1 ? args[1] : nullptr);
  re2::RE2::Arg a2(argc > 2 ? args[2] : nullptr);
  re2::RE2::Arg a3(argc > 3 ? args[3] : nullptr);
  const re2::RE2::Arg* const wrapped_args[] = {&a0, &a1, &a2, &a3};
  CHECK_LE(argc, 4);

  bool result = re2::RE2::FindAndConsumeN(input, pattern, wrapped_args, argc);

  if (skipped_input && result) {
    size_t bytes_skipped = args[0]->data() - old_input.data();
    *skipped_input = old_input.substr(0, bytes_skipped);
  }
  return result;
}

// All |match_groups| need to be of type std::string_view*.
template <typename... Arg>
bool FindAndConsumeAndGetSkipped(std::string_view* input,
                                 const re2::RE2& pattern,
                                 std::string_view* skipped_input,
                                 Arg*... match_groups) {
  std::string_view* args[] = {match_groups...};
  return FindAndConsumeAndGetSkippedN(input, pattern, skipped_input, args,
                                      std::size(args));
}

bool HasRepeatedChar(std::string_view text, char c) {
  return std::adjacent_find(text.begin(), text.end(), [c](char c1, char c2) {
           return (c1 == c) && (c2 == c);
         }) != text.end();
}

// The following MAC addresses will not be redacted as they are not specific
// to a device but have general meanings.
const char* const kUnredactedMacAddresses[] = {
    "00:00:00:00:00:00",  // ARP failure result MAC.
    "ff:ff:ff:ff:ff:ff",  // Broadcast MAC.
};
constexpr size_t kNumUnredactedMacs = std::size(kUnredactedMacAddresses);

bool IsFeatureEnabled(const base::Feature& feature) {
  return base::FeatureList::GetInstance()
             ? base::FeatureList::IsEnabled(feature)
             : feature.default_state == base::FEATURE_ENABLED_BY_DEFAULT;
}
}  // namespace

RedactionTool::RedactionTool(const char* const* first_party_extension_ids)
    : RedactionTool(first_party_extension_ids,
                    RedactionToolMetricsRecorder::Create()) {}

RedactionTool::RedactionTool(
    const char* const* first_party_extension_ids,
    std::unique_ptr<RedactionToolMetricsRecorder> metrics_recorder)
    : first_party_extension_ids_(first_party_extension_ids),
      metrics_recorder_(std::move(metrics_recorder)) {
  CHECK(metrics_recorder_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
  // Identity-map these, so we don't mangle them.
  for (const char* mac : kUnredactedMacAddresses) {
    mac_addresses_[mac] = mac;
  }
}

RedactionTool::~RedactionTool() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::map<PIIType, std::set<std::string>> RedactionTool::Detect(
    const std::string& input) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AssertLongCPUWorkAllowed();

  std::map<PIIType, std::set<std::string>> detected;

  if (IsFeatureEnabled(features::kEnableCreditCardRedaction)) {
    RedactCreditCardNumbers(input, &detected);
  }
  RedactMACAddresses(input, &detected);
  // This function will add to |detected| only on Chrome OS as Android app
  // storage paths are only detected for Chrome OS.
  RedactAndroidAppStoragePaths(input, &detected);
  DetectWithCustomPatterns(input, &detected);
  // Do hashes last since they may appear in URLs and they also prevent us from
  // properly recognizing the Android storage paths.
  RedactHashes(input, &detected);
  if (IsFeatureEnabled(features::kEnableIbanRedaction)) {
    RedactIbans(input, &detected);
  }
  return detected;
}

std::string RedactionTool::Redact(const std::string& input,
                                  const base::Location& location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return RedactAndKeepSelected(input, /*pii_types_to_keep=*/{}, location);
}

std::string RedactionTool::RedactAndKeepSelected(
    const std::string& input,
    const std::set<PIIType>& pii_types_to_keep,
    const base::Location& location) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AssertLongCPUWorkAllowed();

  RedactionToolCaller caller = GetCaller(location);
  metrics_recorder_->RecordRedactionToolCallerHistogram(caller);
  const base::TimeTicks redaction_start = base::TimeTicks::Now();

  // Copy |input| so we can modify it.
  std::string redacted = input;

  // Do this before MAC addresses as credit cards can use the - as identifier as
  // well and the length could also match a MAC address. Since the credit card
  // check does additional validation against issuer length and Luhns checksum
  // the number of false positives should be lower when ordered like this.
  if (IsFeatureEnabled(features::kEnableCreditCardRedaction) &&
      pii_types_to_keep.find(PIIType::kCreditCard) == pii_types_to_keep.end()) {
    redacted = RedactCreditCardNumbers(std::move(redacted), nullptr);
  }
  if (pii_types_to_keep.find(PIIType::kMACAddress) == pii_types_to_keep.end()) {
    redacted = RedactMACAddresses(std::move(redacted), nullptr);
  }
  if (pii_types_to_keep.find(PIIType::kAndroidAppStoragePath) ==
      pii_types_to_keep.end()) {
    redacted = RedactAndroidAppStoragePaths(std::move(redacted), nullptr);
  }

  redacted = RedactAndKeepSelectedCustomPatterns(std::move(redacted),
                                                 pii_types_to_keep);

  // Do hashes last since they may appear in URLs and they also prevent us
  // from properly recognizing the Android storage paths.
  if (pii_types_to_keep.find(PIIType::kStableIdentifier) ==
      pii_types_to_keep.end()) {
    // URLs and Android storage paths will be partially redacted (only hashes)
    // if |pii_types_to_keep| contains PIIType::kURL or
    // PIIType::kAndroidAppStoragePath and not PIIType::kStableIdentifier.
    redacted = RedactHashes(std::move(redacted), nullptr);
  }
  if (IsFeatureEnabled(features::kEnableIbanRedaction) &&
      pii_types_to_keep.find(PIIType::kIBAN) == pii_types_to_keep.end()) {
    redacted = RedactIbans(std::move(redacted), nullptr);
  }

  metrics_recorder_->RecordTimeSpentRedactingHistogram(base::TimeTicks::Now() -
                                                       redaction_start);

  return redacted;
}

void RedactionTool::EnableCreditCardRedaction(const bool enabled) {
  redact_credit_cards_ = enabled;
}

RE2* RedactionTool::GetRegExp(const std::string& pattern) {
  if (regexp_cache_.find(pattern) == regexp_cache_.end()) {
    RE2::Options options;
    // set_multiline of pcre is not supported by RE2, yet.
    options.set_dot_nl(true);  // Dot matches a new line.
    std::unique_ptr<RE2> re = std::make_unique<RE2>(pattern, options);
    DCHECK_EQ(re2::RE2::NoError, re->error_code()) << "Failed to parse:\n"
                                                   << pattern << "\n"
                                                   << re->error();
    regexp_cache_[pattern] = std::move(re);
  }
  return regexp_cache_[pattern].get();
}

std::string RedactionTool::RedactMACAddresses(
    const std::string& input,
    std::map<PIIType, std::set<std::string>>* detected) {
  // This regular expression finds the next MAC address. It splits the data into
  // an OUI (Organizationally Unique Identifier) part and a NIC (Network
  // Interface Controller) specific part. We also match on dash and underscore
  // because we have seen instances of both of those occurring.

  RE2* mac_re = GetRegExp(
      "([0-9a-fA-F][0-9a-fA-F][:\\-_]"
      "[0-9a-fA-F][0-9a-fA-F][:\\-_]"
      "[0-9a-fA-F][0-9a-fA-F])[:\\-_]("
      "[0-9a-fA-F][0-9a-fA-F][:\\-_]"
      "[0-9a-fA-F][0-9a-fA-F][:\\-_]"
      "[0-9a-fA-F][0-9a-fA-F])");

  std::string result;
  result.reserve(input.size());

  // Keep consuming, building up a result string as we go.
  std::string_view text(input);
  std::string_view skipped, oui, nic;
  static const char kMacSeparatorChars[] = "-_";
  while (FindAndConsumeAndGetSkipped(&text, *mac_re, &skipped, &oui, &nic)) {
    // Look up the MAC address in the hash. Force the separator to be a colon
    // so that the same MAC with a different format will match in all cases.
    std::string oui_string = base::ToLowerASCII(oui);
    base::ReplaceChars(oui_string, kMacSeparatorChars, ":", &oui_string);
    std::string nic_string = base::ToLowerASCII(nic);
    base::ReplaceChars(nic_string, kMacSeparatorChars, ":", &nic_string);
    std::string mac = oui_string + ":" + nic_string;
    std::string replacement_mac = mac_addresses_[mac];
    if (replacement_mac.empty()) {
      // If not found, build up a replacement MAC address by generating a new
      // NIC part.
      int mac_id = mac_addresses_.size() - kNumUnredactedMacs;
      replacement_mac = base::StringPrintf("(MAC OUI=%s IFACE=%d)",
                                           oui_string.c_str(), mac_id);
      mac_addresses_[mac] = replacement_mac;
    }
    if (detected != nullptr) {
      (*detected)[PIIType::kMACAddress].insert(mac);
    }
    result.append(skipped);
    result += replacement_mac;
    metrics_recorder_->RecordPIIRedactedHistogram(PIIType::kMACAddress);
  }

  result.append(text);

  return result;
}

std::string RedactionTool::RedactHashes(
    const std::string& input,
    std::map<PIIType, std::set<std::string>>* detected) {
  // This will match hexadecimal strings from length 32 to 64 that have a word
  // boundary at each end. We then check to make sure they are one of our valid
  // hash lengths before replacing.
  // NOTE: There are some occurrences in the dump data (specifically modetest)
  // where relevant data is formatted with 32 hex chars on a line. In this case,
  // it is preceded by at least 3 whitespace chars, so check for that and in
  // that case do not redact.
  RE2* hash_re = GetRegExp(R"((\s*)\b([0-9a-fA-F]{4})([0-9a-fA-F]{28,60})\b)");

  std::string result;
  result.reserve(input.size());

  // Keep consuming, building up a result string as we go.
  std::string_view text(input);
  std::string_view skipped, pre_whitespace, hash_prefix, hash_suffix;
  while (FindAndConsumeAndGetSkipped(&text, *hash_re, &skipped, &pre_whitespace,
                                     &hash_prefix, &hash_suffix)) {
    result.append(skipped);
    result.append(pre_whitespace);

    // Check if it's a valid length for our hashes or if we need to skip due to
    // the whitespace check.
    size_t hash_length = 4 + hash_suffix.length();
    if ((hash_length != 32 && hash_length != 40 && hash_length != 64) ||
        (hash_length == 32 && pre_whitespace.length() >= 3)) {
      // This is not a hash string, skip it.
      result.append(hash_prefix);
      result.append(hash_suffix);
      continue;
    }

    // Look up the hash value address in the map of replacements.
    std::string hash_prefix_string = base::ToLowerASCII(hash_prefix);
    std::string hash = hash_prefix_string + base::ToLowerASCII(hash_suffix);
    std::string replacement_hash = hashes_[hash];
    if (replacement_hash.empty()) {
      // If not found, build up a replacement value.
      replacement_hash = base::StringPrintf(
          "(HASH:%s %zd)", hash_prefix_string.c_str(), hashes_.size());
      hashes_[hash] = replacement_hash;
    }
    if (detected != nullptr) {
      (*detected)[PIIType::kStableIdentifier].insert(hash);
    }

    result += replacement_hash;

    metrics_recorder_->RecordPIIRedactedHistogram(PIIType::kStableIdentifier);
  }

  result.append(text);

  return result;
}

std::string RedactionTool::RedactAndroidAppStoragePaths(
    const std::string& input,
    std::map<PIIType, std::set<std::string>>* detected) {
  // We only use this on Chrome OS and there's differences in the API for
  // FilePath on Windows which prevents this from compiling, so only enable this
  // code for Chrome OS.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string result;
  result.reserve(input.size());

  // This is for redacting Android data paths included in 'android_app_storage'
  // and 'audit_log' output. <app_specific_path> in the following data paths
  // will be redacted.
  // - /data/data/<package_name>/<app_specific_path>
  // - /data/app/<package_name>/<app_specific_path>
  // - /data/user_de/<number>/<package_name>/<app_specific_path>
  // These data paths are preceded by "/home/root/<user_hash>/android-data" in
  // 'android_app_storage' output, and preceded by "path=" or "exe=" in
  // 'audit_log' output.
  RE2* path_re =
      GetRegExp(R"((?m)((path=|exe=|/home/root/[\da-f]+/android-data))"
                R"(/data/(data|app|user_de/\d+)/[^/\n]+)(/[^\n\s]+))");

  // Keep consuming, building up a result string as we go.
  std::string_view text(input);
  std::string_view skipped;
  std::string_view path_prefix;   // path before app_specific;
  std::string_view pre_data;      // (path=|exe=|/home/root/<hash>/android-data)
  std::string_view post_data;     // (data|app|user_de/\d+)
  std::string_view app_specific;  // (/[^\n\s]+)
  while (FindAndConsumeAndGetSkipped(&text, *path_re, &skipped, &path_prefix,
                                     &pre_data, &post_data, &app_specific)) {
    // We can record these parts as-is.
    result.append(skipped);
    result.append(path_prefix);

    // |app_specific| has to be redacted. First, convert it into components,
    // and then redact each component as follows:
    // - If the component has a non-ASCII character, change it to '*'.
    // - Otherwise, remove all the characters in the component but the first
    //   one.
    // - If the original component has 2 or more bytes, add '_'.
    const base::FilePath path(app_specific);
    std::vector<std::string> components = path.GetComponents();
    DCHECK(!components.empty());

    auto it = components.begin() + 1;  // ignore the leading slash
    for (; it != components.end(); ++it) {
      const auto& component = *it;
      DCHECK(!component.empty());
      result += '/';
      result += (base::IsStringASCII(component) ? component[0] : '*');
      if (component.length() > 1) {
        result += '_';
      }
    }
    if (detected != nullptr) {
      (*detected)[PIIType::kAndroidAppStoragePath].emplace(app_specific);
    }
    metrics_recorder_->RecordPIIRedactedHistogram(
        PIIType::kAndroidAppStoragePath);
  }

  result.append(text);

  return result;
#else
  return input;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

std::string RedactionTool::RedactCreditCardNumbers(
    const std::string& input,
    std::map<PIIType, std::set<std::string>>* detected) {
  std::string result;
  result.reserve(input.size());

  RE2* cc_re = GetRegExp(
      "[^\\d\\n]{1,5}[ :='\"]"  // pre sequence: Make sure we're not
                                // matching a memory dump or in some
                                // continuous string of numbers.
      "((?:[\\d -]){12,37})"    // sequence: Creditcard length is 12-19 and we
                                // allow up to one separation character (space
                                // or hyphen) between each of them.
      "(\n|\\D{2,3})");         // post sequence: Not trying to match inside a
                                // continuous number block, so the characters
                                // after the potential match should either be a
                                // newline or 2-3 non digits.

  std::string_view text(input);
  std::string_view skipped;
  std::string_view sequence;
  std::string_view post_sequence;

  while (FindAndConsumeAndGetSkipped(&text, *cc_re, &skipped, &sequence,
                                     &post_sequence)) {
    result.append(skipped);
    metrics_recorder_->RecordCreditCardRedactionHistogram(
        CreditCardDetection::kRegexMatch);

    // Timestamps in ms have a surprisingly high number of false positives.
    // Also log entries but those usually only match if there are several spaces
    // tying unrelated numbers together.
    if (post_sequence.find("ms") != std::string_view::npos) {
      metrics_recorder_->RecordCreditCardRedactionHistogram(
          CreditCardDetection::kTimestamp);
      result.append(sequence);
      result.append(post_sequence);
      continue;
    }

    if (HasRepeatedChar(sequence, ' ') || HasRepeatedChar(sequence, '-')) {
      metrics_recorder_->RecordCreditCardRedactionHistogram(
          CreditCardDetection::kRepeatedChars);
      result.append(sequence);
      result.append(post_sequence);
      continue;
    }

    const std::u16string stripped_number =
        autofill::StripCardNumberSeparators(base::UTF8ToUTF16(sequence));
    const std::string u8number = base::UTF16ToUTF8(stripped_number);

    const auto cc_it = credit_cards_.find(u8number);
    if (cc_it != credit_cards_.cend()) {
      result += cc_it->second;
      result.append(post_sequence);
      metrics_recorder_->RecordCreditCardRedactionHistogram(
          CreditCardDetection::kValidated);
      metrics_recorder_->RecordPIIRedactedHistogram(PIIType::kCreditCard);
      continue;
    }

    const bool only_zeros =
        stripped_number.find_first_not_of(u'0', 0) == std::u16string::npos;
    if (!only_zeros && autofill::IsValidCreditCardNumber(stripped_number)) {
      metrics_recorder_->RecordCreditCardRedactionHistogram(
          CreditCardDetection::kValidated);
      const auto& [it, success] = credit_cards_.emplace(
          u8number,
          base::StrCat({"(CREDITCARD: ",
                        base::NumberToString(credit_cards_.size() + 1), ")"}));
      if (redact_credit_cards_) {
        metrics_recorder_->RecordPIIRedactedHistogram(PIIType::kCreditCard);
        result += it->second;
      } else {
        result.append(sequence);
      }
      if (detected) {
        (*detected)[PIIType::kCreditCard].insert(it->first);
      }
    } else {
      metrics_recorder_->RecordCreditCardRedactionHistogram(
          CreditCardDetection::kDoesntValidate);
      result.append(sequence);
    }
    result.append(post_sequence);
  }

  result.append(text);

  return result;
}

std::string RedactionTool::RedactIbans(
    const std::string& input,
    std::map<PIIType, std::set<std::string>>* detected) {
  std::string result;
  result.reserve(input.size());

  RE2* iban_re = GetRegExp(
      "(:| )"
      "((?:A[DELAOTZ]|B[AEFGHIJR]|C[HIMRVYZ]|D[EKOZ]|E[ES]|F[IOR]|G[BEILRT]|"
      "H[RU]|I[ELRST]|JO|K[WZ]|L[BITUV]|M[CDEGKLRTUZ]|N[LO]|P[KLST]|QA|R[OS]|"
      "S[AEIKMN]|T[NR]|UA|VG|XK)(?:\\d{2})[ -]?(?:[ \\-A-Z0-9]){11,30})"
      "([^a-zA-Z0-9_\\-\\+=/])");

  std::string_view text(input);
  std::string_view skipped;
  std::string_view pre_separating_char;
  std::string_view iban;
  std::string_view post_separating_char;
  while (FindAndConsumeAndGetSkipped(&text, *iban_re, &skipped,
                                     &pre_separating_char, &iban,
                                     &post_separating_char)) {
    result.append(skipped);
    result.append(pre_separating_char);

    // Validation sequence as per [1].
    //
    // [1]
    // https://en.wikipedia.org/wiki/International_Bank_Account_Number#Validating_the_IBAN

    // Remove the separating characters.
    std::string stripped;
    base::RemoveChars(iban, " -", &stripped);

    if (const auto previous_iban = ibans_.find(stripped);
        previous_iban != ibans_.end()) {
      result += previous_iban->second;
      result.append(post_separating_char);
      metrics_recorder_->RecordPIIRedactedHistogram(PIIType::kIBAN);
      continue;
    }

    // Since the logic later relies on the size of this string not changing use
    // a lambda to initialize the constant.
    const std::string numbers_only = [](std::string_view stripped) {
      // Move the first 2 chars+digits to the back of the string.
      constexpr size_t prefix_offset = 4;
      std::string rearranged = std::string(stripped.substr(prefix_offset));
      rearranged.append(stripped.substr(0, prefix_offset));

      // Replace letters with two digits, where A = 10, B = 11, ..., Z = 35.
      std::string tmp;
      for (const char c : rearranged) {
        if (base::IsAsciiDigit(c)) {
          tmp.push_back(c);
        } else {
          const char based_char = c - 'A';
          constexpr size_t iban_char_conversion_offset = 10;
          tmp.append(base::NumberToString(static_cast<int>(based_char) +
                                          iban_char_conversion_offset));
        }
      }
      return tmp;
    }(stripped);

    // Calculate the remainder using chunks.
    constexpr size_t chunk_size = 9;

    std::string chunk;
    chunk.reserve(chunk_size);

    unsigned remainder = 0;

    for (size_t remaining = numbers_only.size(); remaining > 0;) {
      const size_t pos = numbers_only.size() - remaining;
      const size_t next_chunk_size =
          std::min(chunk_size - chunk.size(), remaining);

      chunk.append(numbers_only.substr(pos, next_chunk_size));

      const unsigned long chunk_number =
          std::strtoul(chunk.c_str(), nullptr, 10);

      remainder = chunk_number % 97;
      chunk = base::NumberToString(remainder);

      remaining -= next_chunk_size;
    }

    if (remainder != 1) {
      result.append(iban);
      result.append(post_separating_char);
      continue;
    }

    const auto& [it, success] = ibans_.emplace(
        stripped, base::StrCat({"(IBAN: ",
                                base::NumberToString(ibans_.size() + 1), ")"}));
    result += it->second;
    result.append(post_separating_char);

    if (detected != nullptr) {
      (*detected)[PIIType::kIBAN].insert(it->first);
    }

    metrics_recorder_->RecordPIIRedactedHistogram(PIIType::kIBAN);
  }

  result.append(text);

  return result;
}

std::string RedactionTool::RedactAndKeepSelectedCustomPatterns(
    std::string input,
    const std::set<PIIType>& pii_types_to_keep) {
  for (const auto& pattern : kCustomPatternsWithContext) {
    if (pii_types_to_keep.find(pattern.pii_type) == pii_types_to_keep.end()) {
      input = RedactCustomPatternWithContext(input, pattern, nullptr);
    }
  }
  for (const auto& pattern : kCustomPatternsWithoutContext) {
    if (pii_types_to_keep.find(pattern.pii_type) == pii_types_to_keep.end()) {
      input = RedactCustomPatternWithoutContext(input, pattern, nullptr);
    }
  }
  return input;
}

void RedactionTool::DetectWithCustomPatterns(
    std::string input,
    std::map<PIIType, std::set<std::string>>* detected) {
  for (const auto& pattern : kCustomPatternsWithContext) {
    RedactCustomPatternWithContext(input, pattern, detected);
  }
  for (const auto& pattern : kCustomPatternsWithoutContext) {
    RedactCustomPatternWithoutContext(input, pattern, detected);
  }
}

RedactionToolCaller RedactionTool::GetCaller(const base::Location& location) {
  std::string filePath = location.file_name();
  if (filePath.empty() || filePath.c_str() == nullptr) {
    return RedactionToolCaller::kUndetermined;
  }

  std::string fileName = filePath.substr(filePath.find_last_of("/\\") + 1);

  if (filePath.find("support_tool") != std::string::npos) {
    return RedactionToolCaller::kSupportTool;
  } else if (filePath.find("error_reporting") != std::string::npos) {
    return RedactionToolCaller::kErrorReporting;
  } else if (fileName == "redaction_tool_unittest.cc") {
    return RedactionToolCaller::kUnitTest;
  } else if (fileName == "system_log_uploader.cc") {
    return RedactionToolCaller::kSysLogUploader;
  } else if (fileName == "system_logs_fetcher.cc") {
    return RedactionToolCaller::kSysLogFetcher;
  } else if (fileName == "chrome_js_error_report_processor.cc") {
    return RedactionToolCaller::kCrashToolJSErrors;
  } else if (fileName == "crash_collector.cc") {
    return RedactionToolCaller::kCrashTool;
  } else if (fileName == "feedback_common.cc") {
    return RedactionToolCaller::kFeedbackToolUserDescriptions;
  } else if (fileName == "log_source_access_manager.cc") {
    return RedactionToolCaller::kFeedbackToolHotRod;
  } else if (fileName == "system_logs_fetcher.cc") {
    return RedactionToolCaller::kFeedbackToolLogs;
  }
  return RedactionToolCaller::kUnknown;
}

std::string RedactionTool::RedactCustomPatternWithContext(
    const std::string& input,
    const CustomPatternWithAlias& pattern,
    std::map<PIIType, std::set<std::string>>* detected) {
  RE2* re = GetRegExp(pattern.pattern);
  DCHECK_EQ(3, re->NumberOfCapturingGroups());
  std::map<std::string, std::string>* identifier_space =
      &custom_patterns_with_context_[pattern.alias];

  std::string result;
  result.reserve(input.size());

  // Keep consuming, building up a result string as we go.
  std::string_view text(input);
  std::string_view skipped;
  std::string_view pre_matched_id, matched_id, post_matched_id;
  while (FindAndConsumeAndGetSkipped(&text, *re, &skipped, &pre_matched_id,
                                     &matched_id, &post_matched_id)) {
    std::string matched_id_as_string(matched_id);
    std::string replacement_id;

    std::string scrubbed_match;
    if (pattern.pii_type == PIIType::kIPAddress) {
      std::string prematch(skipped);
      prematch.append(pre_matched_id);
      scrubbed_match = MaybeScrubIPAddress(matched_id_as_string);
      if (scrubbed_match == matched_id_as_string ||
          ((strcmp("IPv4", pattern.alias) == 0) &&
           ShouldSkipIPv4Address(prematch))) {
        result.append(skipped);
        result.append(pre_matched_id);
        result.append(matched_id);
        result.append(post_matched_id);
        continue;
      }
    }

    if (identifier_space->count(matched_id_as_string) == 0) {
      // The weird NumberToString trick is because Windows does not like
      // to deal with %zu and a size_t in printf, nor does it support %llu.
      replacement_id = base::StringPrintf(
          "(%s: %s)",
          scrubbed_match.empty() ? pattern.alias : scrubbed_match.c_str(),
          base::NumberToString(identifier_space->size() + 1).c_str());
      (*identifier_space)[matched_id_as_string] = replacement_id;
    } else {
      replacement_id = (*identifier_space)[matched_id_as_string];
    }
    if (detected != nullptr) {
      (*detected)[pattern.pii_type].insert(matched_id_as_string);
    }
    result.append(skipped);
    result.append(pre_matched_id);
    result += replacement_id;
    result.append(post_matched_id);
    metrics_recorder_->RecordPIIRedactedHistogram(pattern.pii_type);
  }
  result.append(text);

  return result;
}

// This takes a |url| argument and returns true if the URL is exempt from
// redaction, returns false otherwise.
bool IsUrlExempt(std::string_view url,
                 const char* const* first_party_extension_ids) {
  // We do not exempt anything with a query parameter.
  if (url.find("?") != std::string_view::npos) {
    return false;
  }

  // Last part of an SELinux context is misdetected as a URL.
  // e.g. "u:object_r:system_data_file:s0:c512,c768"
  if (url.starts_with("file:s0")) {
    return true;
  }

  // Check for chrome:// URLs that are exempt.
  if (url.starts_with("chrome://")) {
    // We allow everything in chrome://resources/.
    if (url.starts_with("chrome://resources/")) {
      return true;
    }

    // We allow chrome://*/crisper.js.
    if (url.ends_with("/crisper.js")) {
      return true;
    }

    return false;
  }

  if (!first_party_extension_ids) {
    return false;
  }

  // Exempt URLs of the format chrome-extension://<first-party-id>/*.js
  if (!url.starts_with("chrome-extension://")) {
    return false;
  }

  // These must end with a .js extension.
  if (!url.ends_with(".js")) {
    return false;
  }

  int i = 0;
  const char* test_id = first_party_extension_ids[i];
  const std::string_view url_sub =
      url.substr(sizeof("chrome-extension://") - 1);
  while (test_id) {
    if (url_sub.starts_with(test_id)) {
      return true;
    }
    test_id = first_party_extension_ids[++i];
  }
  return false;
}

std::string RedactionTool::RedactCustomPatternWithoutContext(
    const std::string& input,
    const CustomPatternWithAlias& pattern,
    std::map<PIIType, std::set<std::string>>* detected) {
  RE2* re = GetRegExp(pattern.pattern);
  DCHECK_EQ(1, re->NumberOfCapturingGroups());

  std::map<std::string, std::string>* identifier_space =
      &custom_patterns_without_context_[pattern.alias];

  std::string result;
  result.reserve(input.size());

  // Keep consuming, building up a result string as we go.
  std::string_view text(input);
  std::string_view skipped;
  std::string_view matched_id;
  while (FindAndConsumeAndGetSkipped(&text, *re, &skipped, &matched_id)) {
    result.append(skipped);

    if (IsUrlExempt(matched_id, first_party_extension_ids_)) {
      result.append(matched_id);
      continue;
    }

    const std::string matched_id_as_string(matched_id);
    if (const auto previous_replacement =
            identifier_space->find(matched_id_as_string);
        previous_replacement != identifier_space->end()) {
      metrics_recorder_->RecordPIIRedactedHistogram(pattern.pii_type);
      result.append(previous_replacement->second);
      continue;
    }

    const std::string scrubbed_match =
        MaybeScrubIPAddress(matched_id_as_string);
    if (scrubbed_match == matched_id_as_string) {
      result.append(matched_id);
      continue;
    }

    // The weird NumberToString trick is because Windows does not like
    // to deal with %zu and a size_t in printf, nor does it support %llu.
    const auto [redacted_pair, success] = identifier_space->insert_or_assign(
        matched_id_as_string,
        base::StringPrintf(
            "(%s: %s)",
            scrubbed_match.empty() ? pattern.alias : scrubbed_match.c_str(),
            base::NumberToString(identifier_space->size() + 1).c_str()));
    if (detected != nullptr) {
      (*detected)[pattern.pii_type].insert(matched_id_as_string);
    }

    result += redacted_pair->second;
    metrics_recorder_->RecordPIIRedactedHistogram(pattern.pii_type);
  }
  result.append(text);

  return result;
}

RedactionToolContainer::RedactionToolContainer(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const char* const* first_party_extension_ids)
    : redactor_(new RedactionTool(first_party_extension_ids)),
      task_runner_(task_runner) {}

RedactionToolContainer::RedactionToolContainer(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    const char* const* first_party_extension_ids,
    std::unique_ptr<RedactionToolMetricsRecorder> metrics_recorder)
    : redactor_(new RedactionTool(first_party_extension_ids,
                                  std::move(metrics_recorder))),
      task_runner_(task_runner) {}

RedactionToolContainer::~RedactionToolContainer() {
  task_runner_->DeleteSoon(FROM_HERE, std::move(redactor_));
}

RedactionTool* RedactionToolContainer::Get() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return redactor_.get();
}

}  // namespace redaction
