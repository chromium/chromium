// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_I18N_PARSING_EXPRESSION_COMPONENTS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_I18N_PARSING_EXPRESSION_COMPONENTS_H_

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/field_types.h"

namespace autofill::i18n_model_definition {

// Results of a parsing operation. If parsing was successful,
// contains the matching results, keyed by the name of the capture group with
// the captured substrings as the value. Otherwise this is a `nullopt`.
using ValueParsingResults =
    std::optional<base::flat_map<std::string, std::string>>;

// An AutofillParsingProcess is a structure that represents a parsing process
// that transforms unstructured data model values into structured information.
// Each implementation of this class expresses a different parsing logic by
// defining its own implementation of the `Parse` method.
// As an example, a parsing process can transform an address text like:
//     “Avenida Mem de Sá, 1234
//     apto 12
//     1 andar
//     referência: foo”
// Into structured information:
//     ADDRESS_HOME_STREET_NAME: "Avenida Mem de Sá"
//     ADDRESS_HOME_HOUSE_NUMBER: "1234"
//     ADDRESS_HOME_APT_NUM: "apto 12"
//     ADDRESS_HOME_FLOOR: "1"
//     ADDRESS_HOME_LANDMARK: "foo"
class AutofillParsingProcess {
 public:
  constexpr AutofillParsingProcess() = default;
  AutofillParsingProcess(const AutofillParsingProcess& other) = delete;
  AutofillParsingProcess& operator=(const AutofillParsingProcess& right) =
      delete;
  virtual constexpr ~AutofillParsingProcess() = default;

  // Parses `value` and returns the extracted field type matches.
  virtual ValueParsingResults Parse(std::string_view value) const = 0;
};

// A Decomposition parsing process attempts to match an entire string (unless
// anchor_beginning or anchor_end create exceptions) to a parsing expression,
// and then extracts the captured field type values.
class Decomposition : public AutofillParsingProcess {
 public:
  // Note that `parsing_regex` needs to survive the lifetime of the
  // Decomposition.
  constexpr Decomposition(std::string_view parsing_regex,
                          bool anchor_beginning,
                          bool anchor_end)
      : parsing_regex_(parsing_regex),
        anchor_beginning_(anchor_beginning),
        anchor_end_(anchor_end) {}
  Decomposition(const Decomposition&) = delete;
  Decomposition& operator=(const Decomposition&) = delete;
  constexpr ~Decomposition() override;

  ValueParsingResults Parse(std::string_view value) const override;

 private:
  const std::string_view parsing_regex_;
  const bool anchor_beginning_ = true;
  const bool anchor_end_ = true;
};

constexpr Decomposition::~Decomposition() = default;

// A DecompositionCascade enables us to try one Decomposition after the next
// until we have found a match. It can be fitted with a condition to only use it
// in case the condition is fulfilled. The lack of a condition is expressed by
// an empty string.
class DecompositionCascade : public AutofillParsingProcess {
 public:
  // Note that `condition_regex` and `alternatives` need to survive the lifetime
  // of the DecompositionCascade.
  constexpr DecompositionCascade(
      std::string_view condition_regex,
      base::span<const AutofillParsingProcess* const> alternatives)
      : condition_regex_(condition_regex), alternatives_(alternatives) {}
  DecompositionCascade(const DecompositionCascade&) = delete;
  DecompositionCascade& operator=(const DecompositionCascade&) = delete;
  constexpr ~DecompositionCascade() override;

  ValueParsingResults Parse(std::string_view value) const override;

 private:
  const std::string_view condition_regex_;
  const base::span<const AutofillParsingProcess* const> alternatives_;
};

constexpr DecompositionCascade::~DecompositionCascade() = default;

// An ExtractPart parsing process attempts to match a string to a
// parsing expression, and then extracts the captured field type values. It can
// be fitted with a condition to only use it in case the condition is fulfilled.
// The lack of a condition is expressed by an empty string.
// While a Decomposition attempts to match the entire string, ExtractPart is
// designed to contains an anchor term (e.g. "Apt.") after which information
// should be extracted (the apartment number).
class ExtractPart : public AutofillParsingProcess {
 public:
  // Note that `condition_regex` and `parsing_regex` need to survive the
  // lifetime of the DecompositionCascade.
  constexpr ExtractPart(std::string_view condition_regex,
                        std::string_view parsing_regex)
      : condition_regex_(condition_regex), parsing_regex_(parsing_regex) {}

  ExtractPart(const ExtractPart&) = delete;
  ExtractPart& operator=(const ExtractPart&) = delete;
  constexpr ~ExtractPart() override;

  ValueParsingResults Parse(std::string_view value) const override;

 private:
  const std::string_view condition_regex_;
  const std::string_view parsing_regex_;
};

constexpr ExtractPart::~ExtractPart() = default;

// Unlike for a DecompositionCascade, ExtractParts does not follow the "the
// first match wins" principle but applies all matching attempts in sequence so
// the last match wins. This also enables extracting different data (e.g. an
// apartment and a floor) in a sequence of ExtractPart operations. It can also
// be fitted with a condition to only use it in case the condition is fulfilled.
// The lack of a condition is expressed by an empty string.
class ExtractParts : public AutofillParsingProcess {
 public:
  // Note that `condition_regex` and `pieces` need to survive the lifetime of
  // the ExtractParts.
  constexpr ExtractParts(std::string_view condition_regex,
                         base::span<const ExtractPart* const> pieces)
      : condition_regex_(condition_regex), pieces_(pieces) {}
  ExtractParts(const ExtractParts&) = delete;
  ExtractParts& operator=(const ExtractParts&) = delete;
  constexpr ~ExtractParts() override;

  ValueParsingResults Parse(std::string_view value) const override;

 private:
  const std::string_view condition_regex_;
  const base::span<const ExtractPart* const> pieces_;
};

constexpr ExtractParts::~ExtractParts() = default;

}  // namespace autofill::i18n_model_definition

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_AUTOFILL_I18N_PARSING_EXPRESSION_COMPONENTS_H_
