// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_ANONYMIZER_TOOL_H_
#define COMPONENTS_FEEDBACK_ANONYMIZER_TOOL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"

namespace re2 {
class RE2;
}

namespace feedback {

struct CustomPatternWithAlias {
  // A string literal used in anonymized tests. Matches to the |pattern| are
  // replaced with <|alias|: 1>, <|alias|: 2>, ...
  const char* alias;
  // A RE2 regexp used in the replacing logic. Matches will be replaced by the
  // alias reference described above.
  const char* pattern;
};

class AnonymizerTool {
 public:
  // |first_party_extension_ids| is a null terminated array of all the 1st
  // party extension IDs whose URLs won't be redacted. It is OK to pass null for
  // that value if it's OK to redact those URLs or they won't be present.
  AnonymizerTool(const char* const* first_party_extension_ids);
  ~AnonymizerTool();

  // Returns an anonymized version of |input|. PII-sensitive data (such as MAC
  // addresses) in |input| is replaced with unique identifiers.
  // This is an expensive operation. Make sure not to execute this on the UI
  // thread.
  std::string Anonymize(const std::string& input);

 private:
  friend class AnonymizerToolTest;

  re2::RE2* GetRegExp(const std::string& pattern);

  std::string AnonymizeMACAddresses(const std::string& input);
  std::string AnonymizeAndroidAppStoragePaths(const std::string& input);
  std::string AnonymizeHashes(const std::string& input);
  std::string AnonymizeCustomPatterns(std::string input);
  std::string AnonymizeCustomPatternWithContext(
      const std::string& input,
      const CustomPatternWithAlias& pattern);
  std::string AnonymizeCustomPatternWithoutContext(
      const std::string& input,
      const CustomPatternWithAlias& pattern);

  // Null-terminated list of first party extension IDs. We need to have this
  // passed into us because we can't refer to the code where these are defined.
  const char* const* first_party_extension_ids_;  // Not owned.

  // Map of MAC addresses discovered in anonymized strings to anonymized
  // representations. 11:22:33:44:55:66 gets anonymized to
  // [MAC OUI=11:22:33 IFACE=1], where the first three bytes (OUI) represent the
  // manufacturer. The IFACE value is incremented for each newly discovered MAC
  // address.
  std::map<std::string, std::string> mac_addresses_;

  // Map of hashes discovered in anonymized strings to anonymized
  // representations. Hexadecimal strings of length 32, 40 and 64 are considered
  // to be hashes. 11223344556677889900aabbccddeeff gets anonymized to
  // <HASH:1122 1> where the first 2 bytes of the hash are retained as-is and
  // the value after that is incremented for each newly discovered hash.
  std::map<std::string, std::string> hashes_;

  // Like mac addresses, identifiers in custom patterns are anonymized.
  // custom_patterns_with_context_["alias"] contains a map of original
  // identifier to anonymized identifier for custom pattern with the given
  // "alias".  We key on alias to allow different patterns to use the same
  // replacement maps.
  std::map<std::string, std::map<std::string, std::string>>
      custom_patterns_with_context_;
  std::map<std::string, std::map<std::string, std::string>>
      custom_patterns_without_context_;

  // Cache to prevent the repeated compilation of the same regular expression
  // pattern. Key is the string representation of the RegEx.
  std::map<std::string, std::unique_ptr<re2::RE2>> regexp_cache_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(AnonymizerTool);
};

// A container for a AnonymizerTool that is thread-safely ref-countable.
// This is useful for a class that wants to post an async anonymization task
// to a background sequence runner and not deal with its own life-cycle ending
// while the AnonymizerTool is busy on another sequence.
class AnonymizerToolContainer
    : public base::RefCountedThreadSafe<AnonymizerToolContainer> {
 public:
  explicit AnonymizerToolContainer(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const char* const* first_party_extension_ids);

  // Returns a pointer to the instance of this anonymier. May only be called
  // on |task_runner_|.
  AnonymizerTool* Get();

 private:
  friend class base::RefCountedThreadSafe<AnonymizerToolContainer>;
  ~AnonymizerToolContainer();

  std::unique_ptr<AnonymizerTool> anonymizer_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace feedback

#endif  // COMPONENTS_FEEDBACK_ANONYMIZER_TOOL_H_
