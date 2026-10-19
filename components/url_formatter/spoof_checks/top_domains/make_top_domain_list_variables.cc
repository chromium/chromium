// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This binary generates a C++ source file containing the skeletons of the top
// bucket domains suitable for use in the edit distance heuristic, which we
// embed directly into the final Chrome binary. The input is a list of the top
// domains. The output is a single array of skeletons, exposed as
// `base::span<const char* const> kTopBucketEditDistanceSkeletons`.

// The C-strings in the output array are guaranteed to be in lexicographically
// sorted order.

// IMPORTANT: This binary asserts that the output has at most 500 skeletons.

#include <algorithm>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/logging/logging_settings.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/url_formatter/spoof_checks/common_words/common_words_util.h"
#include "components/url_formatter/spoof_checks/top_domains/top_domain_util.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/uspoof.h"

namespace {

// The maximum size of the skeleton array generated in
// top-bucket-domains-inc.cc. If the input has fewer than kMaxDomains eligible
// top bucket domains (e.g. because some are too short), the generated array is
// smaller.
const size_t kMaxDomains = 500;
const char* kTopBucketSeparator = "###END_TOP_BUCKET###";

// Similar to kMaxDomains, but for keywords. Keywords are collected but not
// written to the output.
const size_t kMaxKeywords = 500;

// The minimum length for a keyword for it to be included.
const size_t kMinKeywordLength = 3;

void PrintHelp() {
  std::cout << "make_top_domain_list_variables <input-file>"
            << " <namespace-name> <output-file> [--v=1]" << std::endl;
}

std::string GetSkeleton(const std::string& domain,
                        const USpoofChecker* spoof_checker) {
  UErrorCode status = U_ZERO_ERROR;
  icu::UnicodeString ustr_skeleton;
  uspoof_getSkeletonUnicodeString(spoof_checker, 0 /* not used */,
                                  icu::UnicodeString::fromUTF8(domain),
                                  ustr_skeleton, &status);
  std::string skeleton;
  return U_SUCCESS(status) ? ustr_skeleton.toUTF8String(skeleton) : skeleton;
}

bool ContainsOnlyDigits(const std::string& text) {
  return std::ranges::all_of(text.begin(), text.end(), ::isdigit);
}

}  // namespace

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

#if BUILDFLAG(IS_WIN)
  std::vector<std::string> args;
  base::CommandLine::StringVector wide_args = command_line.GetArgs();
  args.reserve(wide_args.size());
  for (const auto& arg : wide_args) {
    args.push_back(base::WideToUTF8(arg));
  }
#else
  base::CommandLine::StringVector args = command_line.GetArgs();
#endif
  if (args.size() < 3) {
    PrintHelp();
    return 1;
  }

  base::FilePath input_path = base::MakeAbsoluteFilePath(
      base::FilePath::FromUTF8Unsafe(UNSAFE_TODO(argv[1])));
  if (!base::PathExists(input_path)) {
    LOG(ERROR) << "Input path doesn't exist: " << input_path;
    return 1;
  }

  std::string input_text;
  if (!base::ReadFileToString(input_path, &input_text)) {
    LOG(ERROR) << "Could not read input file: " << input_path;
    return 1;
  }

  std::string namespace_str = UNSAFE_TODO(argv[2]);

  std::vector<std::string> lines = base::SplitString(
      input_text, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  base::i18n::InitializeICU();
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<USpoofChecker, decltype(&uspoof_close)> spoof_checker(
      uspoof_open(&status), &uspoof_close);
  if (U_FAILURE(status)) {
    std::cerr << "Failed to create an ICU uspoof_checker due to "
              << u_errorName(status) << ".\n";
    return 1;
  }

  absl::flat_hash_set<std::string> skeletons;
  absl::flat_hash_set<std::string> keywords;

  for (std::string line : lines) {
    if (skeletons.size() >= kMaxDomains && keywords.size() >= kMaxKeywords) {
      break;
    }
    base::TrimWhitespaceASCII(line, base::TRIM_ALL, &line);

    if (line == kTopBucketSeparator) {
      break;
    }

    if (line.empty() || line[0] == '#') {
      continue;
    }

    if (skeletons.size() < kMaxDomains &&
        url_formatter::top_domains::IsEditDistanceCandidate(line)) {
      skeletons.insert(GetSkeleton(line, spoof_checker.get()));
    }

    if (keywords.size() < kMaxKeywords) {
      std::string keyword =
          url_formatter::top_domains::HostnameWithoutRegistry(line);
      CHECK(keyword.find('.') == std::string::npos);
      if (keyword.find('-') == std::string::npos &&
          keyword.length() >= kMinKeywordLength &&
          !ContainsOnlyDigits(keyword) &&
          !url_formatter::common_words::IsCommonWord(keyword)) {
        keywords.insert(std::move(keyword));
      }
    }
  }

  // The generated array can't be empty: zero-length arrays are ill-formed.
  CHECK(!skeletons.empty());
  CHECK_LE(skeletons.size(), kMaxDomains);
  CHECK_LE(keywords.size(), kMaxKeywords);

  std::vector<std::string> sorted_skeletons(
      std::make_move_iterator(skeletons.begin()),
      std::make_move_iterator(skeletons.end()));
  std::sort(sorted_skeletons.begin(), sorted_skeletons.end());

  std::ostringstream output_stream;
  output_stream
      << R"(#include "components/url_formatter/spoof_checks/top_domains/)"
      << namespace_str << R"(.h"
namespace )"
      << namespace_str << R"( {
namespace {
const char* const kTopBucketEditDistanceSkeletonsArray[] = {
)";

  for (const std::string& skeleton : sorted_skeletons) {
    output_stream << base::StrCat({"\"", skeleton, "\"", ",\n"});
  }
  output_stream << R"(};
}  // namespace

extern constexpr base::span<const char* const>
    kTopBucketEditDistanceSkeletons = kTopBucketEditDistanceSkeletonsArray;

}  // namespace
)";

  std::string output = output_stream.str();

  base::FilePath output_path =
      base::FilePath::FromUTF8Unsafe(UNSAFE_TODO(argv[3]));
  if (!base::WriteFile(output_path, output)) {
    LOG(ERROR) << "Failed to write output: " << output_path;
    return 1;
  }
  return 0;
}
