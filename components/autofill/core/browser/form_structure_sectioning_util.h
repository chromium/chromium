// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_SECTIONING_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_SECTIONING_UTIL_H_

#include <stdint.h>
#include <memory>

#include "base/containers/span.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/common/signatures.h"

namespace autofill {

// Sectioning is the process of splitting a form into logical groups (e.g.
// shipping, billing, credit card section) which are filled separately.
//
// After this function has finished, the following conditions hold:
//  1. Every field with an autocomplete attribute section S is in section S.
//  2. All credit card fields are in one, distinct section, unless they have a
//     valid autocomplete attribute section.
//  3. All other fields that are focusable or <select> fields are partitioned
//     into intervals, each of which is a section, and starts with a focusable
//     field (i.e., it does not start with an unfocusable <select> field).
//  4. All remaining fields are in one, distinct section.
//
// The basic idea of interval partitioning is to start a new section when the
// same field type appears repeatedly. See `BelongsToCurrentSection()` for the
// details.
//
// The motivation behind the special handling of credit card fields is that
// credit card forms frequently contain multiple fields of the same type, some
// of which are invisible. In such cases, repeated field types must not start a
// new section.
//
// Example:
//   ------------------------------------------------------+-------------------
//       HTML code                                         |      Section
//   ------------------------------------------------------+-------------------
//   Name:      <input id=1>                               | field 1 based
//   Country:   <input id=2>                               | field 1 based
//   Name:      <input id=3 autocomplete="section-A name"> | A
//   Street:    <input id=4>                               | field 1 based
//   CC number: <input id=5>                               | field 5 based
//   CC number: <input id=6 style="display:none">          | field 5 based
//   Name:      <input id=7>                               | field 7 based
//   Country:   <input id=8>                               | field 7 based
//   CC number: <input id=9>                               | field 5 based
//   ------------------------------------------------------+-------------------
//
// The `kAutofillUseParameterizedSectioning` has boolean feature parameters
// which assign sections in the following way:
// a. `kAutofillSectioningModeIgnoreAutocomplete`: Disables condition 1, i.e.
//    ignores the autocomplete attribute section. Otherwise, conditions 2 to 4
//    apply.
//
//    Example:
//   ------------------------------------------------------+-------------------
//       HTML code                                         |      Section
//   ------------------------------------------------------+-------------------
//   Name:      <input id=1>                               | field 1 based
//   Country:   <input id=2>                               | field 1 based
//   Name:      <input id=3 autocomplete="section-A name"> | field 3 based
//   Street:    <input id=4>                               | field 3 based
//   CC number: <input id=5>                               | field 5 based
//   CC number: <input id=6 style="display:none">          | field 5 based
//   Name:      <input id=7>                               | field 7 based
//   Country:   <input id=8>                               | field 7 based
//   CC number: <input id=9>                               | field 5 based
//   ------------------------------------------------------+-------------------
//
// b. `kAutofillSectioningModeCreateGaps`: The conditions from above apply.
//    Additionally, in condition 3, intervals are split by fields that were
//    sectioned based on conditions 1 and 2.
//
//    Example:
//   ------------------------------------------------------+-------------------
//       HTML code                                         |      Section
//   ------------------------------------------------------+-------------------
//   Name:      <input id=1>                               | field 1 based
//   Country:   <input id=2>                               | field 1 based
//   Name:      <input id=3 autocomplete="section-A name"> | A
//   Street:    <input id=4>                               | field 4 based
//   CC number: <input id=5>                               | field 5 based
//   CC number: <input id=6 style="display:none">          | field 5 based
//   Name:      <input id=7>                               | field 7 based
//   Country:   <input id=8>                               | field 7 based
//   CC number: <input id=9>                               | field 5 based
//   ------------------------------------------------------+-------------------
//
// c. `kAutofillSectioningModeExpand`: The conditions from above apply.
//    Additionally, we look at gaps between fields with sections based on
//    conditions 1 and 2. If the gap is enclosed by the same section, this
//    section is expanded to all the fields inside the gap. If there are no such
//    gaps, this mode degenerates to the regular sectioning algorithm.
//    In the example below, the "Name" and "Country" fields between the last two
//    credit card fields are affected.
//
//    Example:
//   ------------------------------------------------------+-------------------
//       HTML code                                         |      Section
//   ------------------------------------------------------+-------------------
//   Name:      <input id=1>                               | field 1 based
//   Country:   <input id=2>                               | field 1 based
//   Name:      <input id=3 autocomplete="section-A name"> | A
//   Street:    <input id=4>                               | field 1 based
//   CC number: <input id=5>                               | field 5 based
//   CC number: <input id=6 style="display:none">          | field 5 based
//   Name:      <input id=7>                               | field 5 based
//   Country:   <input id=8>                               | field 5 based
//   CC number: <input id=9>                               | field 5 based
//   ------------------------------------------------------+-------------------
void AssignSections(base::span<const std::unique_ptr<AutofillField>> fields);

// Logs UMA and UKM metrics about the `fields`' sections.
// UKM metrics are only logged if `form_interactions_ukm_logger` is available.
void LogSectioningMetrics(
    FormSignature form_signature,
    base::span<const std::unique_ptr<AutofillField>> fields,
    AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger);

// Computes a 32-bit signature of the `fields` sections.
// This is useful for logging Ukm metrics to detect on which sites different
// sectioning algorithms produce different results.
uint32_t ComputeSectioningSignature(
    base::span<const std::unique_ptr<AutofillField>> fields);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_SECTIONING_UTIL_H_
