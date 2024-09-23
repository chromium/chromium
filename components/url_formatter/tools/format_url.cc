// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This binary takes a list of domain names in ASCII or unicode, passes them
// through the IDN decoding algorithm and prints out the result. The list can be
// passed as a text file or via stdin. In both cases, the output is printed as
// (input_domain, output_domain, spoof_check_result) tuples on separate lines.
// spoof_check_result is the string representation of IDNSpoofChecker::Result
// enum with an additional kTopDomainLookalike value.

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/spoof_checks/idn_spoof_checker.h"
#include "components/url_formatter/url_formatter.h"
#include "url/gurl.h"

using url_formatter::IDNConversionResult;
using url_formatter::IDNSpoofChecker;

void PrintUsage(const char* process_name) {
  std::cout << "Usage:" << std::endl;
  std::cout << process_name << " <file>" << std::endl;
  std::cout << std::endl;
  std::cout << "<file> is a text file with one hostname per line." << std::endl;
  std::cout << "Hostnames can be ASCII or unicode. Internationalized domain "
               "can (IDN) be encoded in unicode or punycode."
            << std::endl;
  std::cout << "Each hostname is converted to unicode, if safe. Otherwise, "
            << "ASCII hostnames are printed unchanged and unicode hostnames "
            << "are printed in punycode." << std::endl;
}

std::string SpoofCheckResultToString(IDNSpoofChecker::Result result) {
  switch (result) {
    case IDNSpoofChecker::Result::kNone:
      return "kNone";
    case IDNSpoofChecker::Result::kSafe:
      return "kSafe";
    case IDNSpoofChecker::Result::kICUSpoofChecks:
      return "kICUSpoofChecks";
    case IDNSpoofChecker::Result::kDeviationCharacters:
      return "kDeviationCharacters";
    case IDNSpoofChecker::Result::kTLDSpecificCharacters:
      return "kTLDSpecificCharacters";
    case IDNSpoofChecker::Result::kUnsafeMiddleDot:
      return "kUnsafeMiddleDot";
    case IDNSpoofChecker::Result::kWholeScriptConfusable:
      return "kWholeScriptConfusable";
    case IDNSpoofChecker::Result::kDigitLookalikes:
      return "kDigitLookalikes";
    case IDNSpoofChecker::Result::kNonAsciiLatinCharMixedWithNonLatin:
      return "kNonAsciiLatinCharMixedWithNonLatin";
    case IDNSpoofChecker::Result::kDangerousPattern:
      return "kDangerousPattern";
    default:
      NOTREACHED_IN_MIGRATION();
  };
  return std::string();
}

// Returns the spoof check result as a string. |ascii_domain| must contain
// ASCII characters only. |unicode_domain| is the IDN conversion result
// according to url_formatter. It can be either punycode or unicode.
std::string GetSpoofCheckResult(const std::string& ascii_domain,
                                const std::u16string& unicode_domain) {
  IDNConversionResult result =
      url_formatter::UnsafeIDNToUnicodeWithDetails(ascii_domain);
  std::string spoof_check_result =
      SpoofCheckResultToString(result.spoof_check_result);
  if (result.spoof_check_result == IDNSpoofChecker::Result::kNone) {
    // Input was not punycode.
    return spoof_check_result;
  }
  if (result.spoof_check_result != IDNSpoofChecker::Result::kSafe) {
    return spoof_check_result;
  }
  // If the domain passed all spoof checks but |unicode_domain| is still in
  // punycode, the domain must be a lookalike of a top domain.
  if (base::ASCIIToUTF16(ascii_domain) == unicode_domain) {
    return "kTopDomainLookalike";
  }
  return spoof_check_result;
}

void Convert(std::istream& input) {
  base::i18n::InitializeICU();
  for (std::string line; std::getline(input, line);) {
    CHECK(
        !base::StartsWith(line,
                          "http:", base::CompareCase::INSENSITIVE_ASCII) &&
        !base::StartsWith(line, "https:", base::CompareCase::INSENSITIVE_ASCII))
        << "This binary only accepts hostnames" << line;

    const std::string ascii_hostname =
        base::IsStringASCII(line) ? line : GURL("https://" + line).host();

    // Convert twice, first with spoof checks on, then with spoof checks
    // ignored inside GetSpoofCheckResult(). This is because only the call to
    // UnsafeIDNToUnicodeWithDetails returns information about spoof check
    // results (a quirk of the url_formatter interface).
    const std::u16string converted_hostname =
        url_formatter::IDNToUnicode(ascii_hostname);
    const std::string spoof_check_result =
        GetSpoofCheckResult(ascii_hostname, converted_hostname);
    std::cout << ascii_hostname << ", " << converted_hostname << ", "
              << spoof_check_result << std::endl;
  }
}

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();

  if (cmd->HasSwitch("help")) {
    PrintUsage(argv[0]);
    return 0;
  }

  if (argc > 1) {
    const std::string filename = argv[1];
    std::ifstream input(filename);
    if (!input.good()) {
      LOG(ERROR) << "Could not open file " << filename;
      return -1;
    }
    Convert(input);
  } else {
    Convert(std::cin);
  }

  return EXIT_SUCCESS;
}
