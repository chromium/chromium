// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEEDBACK_REDACTION_TOOL_REDACTION_TOOL_H_
#define COMPONENTS_FEEDBACK_REDACTION_TOOL_REDACTION_TOOL_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/feedback/redaction_tool/redaction_tool_metrics_recorder.h"

namespace re2 {
class RE2;
}

namespace redaction {
namespace features {
COMPONENT_EXPORT(REDACTION_TOOL)
BASE_DECLARE_FEATURE(kEnableCreditCardRedaction);

COMPONENT_EXPORT(REDACTION_TOOL)
BASE_DECLARE_FEATURE(kEnableIbanRedaction);
}  // namespace features

struct CustomPatternWithAlias {
  // A string literal used in redaction tests. Matches to the |pattern| are
  // replaced with <|alias|: 1>, <|alias|: 2>, ...
  const char* alias;
  // A RE2 regexp used in the replacing logic. Matches will be replaced by the
  // alias reference described above.
  const char* pattern;
  // PII category of the data that will be detected using this pattern.
  PIIType pii_type;
};

// Formerly known as AnonymizerTool, RedactionTool provides functions for
// redacting several known PII types, such as MAC address, and redaction
// using custom patterns.
class RedactionTool {
 public:
  // Disallow copy or move
  RedactionTool(const RedactionTool&) = delete;
  RedactionTool& operator=(const RedactionTool&) = delete;
  RedactionTool(RedactionTool&&) = delete;
  RedactionTool& operator=(RedactionTool&&) = delete;

  // |first_party_extension_ids| is a null terminated array of all the 1st
  // party extension IDs whose URLs won't be redacted. It is OK to pass null for
  // that value if it's OK to redact those URLs or they won't be present.
  explicit RedactionTool(const char* const* first_party_extension_ids);
  // The `metrics_recorder` is the instance of recorder that should be used on
  // this instance instead of the default for the platform.
  RedactionTool(const char* const* first_party_extension_ids,
                std::unique_ptr<RedactionToolMetricsRecorder> metrics_recorder);
  ~RedactionTool();

  // Return a map of [PII-sensitive data type -> set of data] that are detected
  // in |input|.
  std::map<PIIType, std::set<std::string>> Detect(const std::string& input);

  // Returns an redacted version of |input|. PII-sensitive data (such as MAC
  // addresses) in |input| is replaced with unique identifiers.
  // This is an expensive operation. Make sure not to execute this on the UI
  // thread.
  // The |location| is automatically determined to be the caller of the function
  // and is used for metrics. It's not passed by the caller.
  std::string Redact(
      const std::string& input,
      const base::Location& location = base::Location::Current());

  // Attempts to redact PII sensitive data from |input| except the data that
  // fits in one of the PII types in |pii_types_to_keep| and returns the
  // redacted version.
  // Note that URLs and Android storage paths may contain hashes. URLs and
  // Android storage paths will be partially redacted (only hashes) if
  // |pii_types_to_keep| contains PIIType::kURL or
  // PIIType::kAndroidAppStoragePath and not PIIType::kHash.
  // The |location| is automatically determined to be the caller of the function
  // and is used for metrics.
  std::string RedactAndKeepSelected(
      const std::string& input,
      const std::set<PIIType>& pii_types_to_keep,
      const base::Location& location = base::Location::Current());

  // Setting `enabled` to `true` redacts credit card numbers in addition to
  // gathering UMA metrics. If not called or `enabled` set to `false` credit
  // cards will only be detected and gathered as UMA metric.
  //
  // This method is only temporary to validate that credit card redaction
  // doesn't produce a large number of false positives in contexts where a lot
  // of numbers are used. As there is no guaranteed context for credit cards we
  // validate with several factors like Luhn checksum and IIN. Even with these
  // in place the possibility of false positives remains, even though reduced by
  // a lot. Until validation is complete, this should thus not be used in
  // automated context like error/crash reporters.
  void EnableCreditCardRedaction(bool enabled);

 private:
  friend class RedactionToolTest;

  re2::RE2* GetRegExp(const std::string& pattern);

  // Redacts MAC addresses from |input| and returns the redacted string. Adds
  // the redacted MAC addresses to |detected| if |detected| is not nullptr.
  std::string RedactMACAddresses(
      const std::string& input,
      std::map<PIIType, std::set<std::string>>* detected);
  // Redacts Android app storage paths from |input| and returns the redacted
  // string. Adds the redacted app storage paths to |detected| if |detected| is
  // not nullptr. This function returns meaningpul output only on Chrome OS.
  std::string RedactAndroidAppStoragePaths(
      const std::string& input,
      std::map<PIIType, std::set<std::string>>* detected);
  // Redacts hashes from |input| and returns the redacted string. Adds the
  // redacted hashes to |detected| if |detected| is not nullptr.
  std::string RedactHashes(const std::string& input,
                           std::map<PIIType, std::set<std::string>>* detected);
  // Redact credit card numbers matching the specific issuer length (if
  // applicable) and matching the Luhn checksum.
  std::string RedactCreditCardNumbers(
      const std::string& input,
      std::map<PIIType, std::set<std::string>>* detected);
  // Redacts IBANs from |input| and returns the redacted string. Adds the
  // redacted IBANs to |detected| under the |PIIType::kIBAN| if |detected| is
  // not a nullptr.
  std::string RedactIbans(const std::string& input,
                          std::map<PIIType, std::set<std::string>>* detected);

  // Redacts PII sensitive data that matches |pattern| from |input| and returns
  // the redacted string. Keeps the PII data that belongs to PII type in
  // |pii_types_to_keep| in the returned string.
  std::string RedactAndKeepSelectedCustomPatterns(
      std::string input,
      const std::set<PIIType>& pii_types_to_keep);

  // Gets the caller of the Redaction tool by looking at the |location|.
  RedactionToolCaller GetCaller(const base::Location& location);

  // Detects PII sensitive data in |input| using custom patterns. Adds the
  // detected PII sensitive data to corresponding PII type key in |detected|.
  void DetectWithCustomPatterns(
      std::string input,
      std::map<PIIType, std::set<std::string>>* detected);
  // Redacts PII sensitive data that matches |pattern| from |input| and returns
  // the redacted string. Adds the redacted PII sensitive data to |detected| if
  // |detected| is not nullptr.
  std::string RedactCustomPatternWithContext(
      const std::string& input,
      const CustomPatternWithAlias& pattern,
      std::map<PIIType, std::set<std::string>>* detected);
  // Redacts PII sensitive data that matches |pattern| from |input| and returns
  // the redacted string. Adds the redacted PII sensitive data to |detected| if
  // |detected| is not nullptr.
  std::string RedactCustomPatternWithoutContext(
      const std::string& input,
      const CustomPatternWithAlias& pattern,
      std::map<PIIType, std::set<std::string>>* detected);

  // Null-terminated list of first party extension IDs. We need to have this
  // passed into us because we can't refer to the code where these are defined.
  raw_ptr<const char* const> first_party_extension_ids_;  // Not owned.

  // Map of MAC addresses discovered in redacted strings to redacted
  // representations. 11:22:33:44:55:66 gets redacted to
  // (MAC OUI=11:22:33 IFACE=1), where the first three bytes (OUI) represent the
  // manufacturer. The IFACE value is incremented for each newly discovered MAC
  // address.
  std::map<std::string, std::string> mac_addresses_;

  // Map of hashes discovered in redacted strings to redacted representations.
  // Hexadecimal strings of length 32, 40 and 64 are considered to be hashes.
  // 11223344556677889900aabbccddeeff gets redacted to (HASH:1122 1) where the
  // first 2 bytes of the hash are retained as-is and the value after that is
  // incremented for each newly discovered hash.
  std::map<std::string, std::string> hashes_;

  // Map of number only representation of a (probably) valid credit card to
  // the redacted representation.
  std::map<std::string, std::string> credit_cards_;

  // Map of IBANs discovered in strings to their redacted representations. The
  // key is stored without any separators.
  std::map<std::string, std::string> ibans_;

  // Like MAC addresses, identifiers in custom patterns are redacted.
  // custom_patterns_with_context_["alias"] contains a map of original
  // identifier to redacted identifier for custom pattern with the given
  // "alias".  We key on alias to allow different patterns to use the same
  // replacement maps.
  std::map<std::string, std::map<std::string, std::string>>
      custom_patterns_with_context_;
  std::map<std::string, std::map<std::string, std::string>>
      custom_patterns_without_context_;

  // Cache to prevent the repeated compilation of the same regular expression
  // pattern. Key is the string representation of the RegEx.
  std::map<std::string, std::unique_ptr<re2::RE2>> regexp_cache_;

  bool redact_credit_cards_ = false;

  std::unique_ptr<RedactionToolMetricsRecorder> metrics_recorder_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// A container for a RedactionTool that is thread-safely ref-countable.
// This is useful for a class that wants to post an async redaction task
// to a background sequence runner and not deal with its own life-cycle ending
// while the RedactionTool is busy on another sequence.
class RedactionToolContainer
    : public base::RefCountedThreadSafe<RedactionToolContainer> {
 public:
  explicit RedactionToolContainer(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const char* const* first_party_extension_ids);
  explicit RedactionToolContainer(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const char* const* first_party_extension_ids,
      std::unique_ptr<RedactionToolMetricsRecorder> metrics_recorder);

  // Returns a pointer to the instance of this redactor. May only be called
  // on |task_runner_|.
  RedactionTool* Get();

 private:
  friend class base::RefCountedThreadSafe<RedactionToolContainer>;
  ~RedactionToolContainer();

  std::unique_ptr<RedactionTool> redactor_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace redaction

#endif  // COMPONENTS_FEEDBACK_REDACTION_TOOL_REDACTION_TOOL_H_
