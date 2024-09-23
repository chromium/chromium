// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/ipp_attribute_validator.h"

#include <cups/ipp.h>

#include <map>
#include <set>
#include <string>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "chrome/services/ipp_parser/public/mojom/ipp_parser.mojom.h"

namespace cups_proxy {

namespace {

using ValueType = ipp_parser::mojom::IppAttributeValue::Tag;

// Represents the type of a single attribute.
struct AttributeDefinition {
  bool multivalued;

  // Internal serialization type.
  ValueType type;
};

}  // namespace

ValidateAttributeResult ValidateAttribute(ipp_op_t ipp_oper_id,
                                          const std::string& name,
                                          ValueType type,
                                          size_t values_count) {
  // Definitions of attributes used in IPP requests.
  static constexpr auto kAttributeDefinitions =
      base::MakeFixedFlatMap<std::string_view, AttributeDefinition>(
          {{"attributes-charset", {false, ValueType::kStrings}},
           {"attributes-natural-language", {false, ValueType::kStrings}},
           {"auth-info", {true, ValueType::kStrings}},
           {"auth-info-required", {true, ValueType::kStrings}},
           {"charset-configured", {false, ValueType::kStrings}},
           {"compression", {false, ValueType::kStrings}},
           {"copies", {false, ValueType::kInts}},
           {"document-format", {false, ValueType::kStrings}},
           {"document-name", {false, ValueType::kStrings}},
           {"finishings", {true, ValueType::kInts}},
           {"ipp-attribute-fidelity", {false, ValueType::kBools}},
           {"job-cancel-after", {false, ValueType::kInts}},
           {"job-hold-until", {false, ValueType::kStrings}},
           {"job-id", {false, ValueType::kInts}},
           {"job-k-limit", {false, ValueType::kInts}},
           {"job-media-progress", {false, ValueType::kInts}},
           {"job-name", {false, ValueType::kStrings}},
           {"job-originating-host-name", {false, ValueType::kStrings}},
           {"job-originating-user-name", {false, ValueType::kStrings}},
           {"job-page-limit", {false, ValueType::kInts}},
           {"job-password", {false, ValueType::kOctets}},
           {"job-printer-state-message", {false, ValueType::kStrings}},
           {"job-printer-state-reasons", {true, ValueType::kStrings}},
           {"job-printer-up-time", {false, ValueType::kInts}},
           {"job-printer-uri", {false, ValueType::kStrings}},
           {"job-priority", {false, ValueType::kInts}},
           {"job-quota-period", {false, ValueType::kInts}},
           {"job-state", {false, ValueType::kInts}},
           {"job-state-reasons", {true, ValueType::kStrings}},
           {"job-uri", {false, ValueType::kStrings}},
           {"last-document", {false, ValueType::kBools}},
           {"limit", {false, ValueType::kInts}},
           {"marker-change-time", {false, ValueType::kInts}},
           {"marker-colors", {true, ValueType::kStrings}},
           {"marker-high-levels", {true, ValueType::kInts}},
           {"marker-levels", {true, ValueType::kInts}},
           {"marker-low-levels", {true, ValueType::kInts}},
           {"marker-message", {false, ValueType::kStrings}},
           {"marker-names", {true, ValueType::kStrings}},
           {"marker-types", {true, ValueType::kStrings}},
           {"media", {false, ValueType::kStrings}},
           {"media-ready", {true, ValueType::kStrings}},
           // This is not a PWG attribute but CUPS supports it for InputSlot
           // selection.
           {"media-source", {true, ValueType::kStrings}},
           {"member-names", {true, ValueType::kStrings}},
           {"member-uris", {true, ValueType::kStrings}},
           {"multiple-document-handling", {false, ValueType::kStrings}},
           {"my-jobs", {false, ValueType::kBools}},
           {"natural-language-configured", {false, ValueType::kStrings}},
           {"number-up", {false, ValueType::kInts}},
           {"orientation-requested", {false, ValueType::kInts}},
           {"output-bin", {false, ValueType::kStrings}},
           {"page-border", {false, ValueType::kStrings}},
           {"page-ranges", {true, ValueType::kInts}},
           {"pages-per-minute", {false, ValueType::kInts}},
           {"pages-per-minute-color", {false, ValueType::kInts}},
           {"print-color-mode", {false, ValueType::kStrings}},
           {"print-quality", {false, ValueType::kInts}},
           {"printer-alert", {true, ValueType::kStrings}},
           {"printer-alert-description", {true, ValueType::kStrings}},
           {"printer-device-id", {false, ValueType::kStrings}},
           {"printer-dns-sd-name", {false, ValueType::kStrings}},
           {"printer-id", {false, ValueType::kInts}},
           {"printer-info", {false, ValueType::kStrings}},
           {"printer-is-accepting-jobs", {false, ValueType::kBools}},
           {"printer-location", {false, ValueType::kStrings}},
           {"printer-make-and-model", {false, ValueType::kStrings}},
           {"printer-more-info", {false, ValueType::kStrings}},
           {"printer-name", {false, ValueType::kStrings}},
           {"printer-resolution", {false, ValueType::kResolutions}},
           {"printer-state", {false, ValueType::kInts}},
           {"printer-state-reasons", {true, ValueType::kStrings}},
           {"printer-type", {false, ValueType::kInts}},
           {"printer-type-mask", {false, ValueType::kInts}},
           {"printer-up-time", {false, ValueType::kInts}},
           {"printer-uri", {false, ValueType::kStrings}},
           {"queued-job-count", {false, ValueType::kInts}},
           {"requested-attributes", {true, ValueType::kStrings}},
           {"requesting-user-name", {false, ValueType::kStrings}},
           {"sides", {false, ValueType::kStrings}},
           {"status-message", {false, ValueType::kStrings}},
           {"time-at-completed", {false, ValueType::kInts}},
           {"time-at-creation", {false, ValueType::kInts}},
           {"time-at-processing", {false, ValueType::kInts}},
           {"which-jobs", {false, ValueType::kStrings}}});

  // Allowed IPP Operations.
  static constexpr auto kOperationIds = base::MakeFixedFlatSet<ipp_op_t>({
      IPP_OP_CANCEL_JOB,
      IPP_OP_CREATE_JOB,
      IPP_OP_CUPS_GET_DEFAULT,
      IPP_OP_CUPS_GET_PPD,
      IPP_OP_CUPS_GET_PRINTERS,
      IPP_OP_GET_JOBS,
      IPP_OP_GET_JOB_ATTRIBUTES,
      IPP_OP_GET_PRINTER_ATTRIBUTES,
      IPP_OP_HOLD_JOB,
      IPP_OP_PAUSE_PRINTER,
      IPP_OP_PRINT_JOB,
      IPP_OP_PRINT_URI,
      IPP_OP_RELEASE_JOB,
      IPP_OP_RESUME_PRINTER,
      IPP_OP_SEND_DOCUMENT,
      IPP_OP_SEND_URI,
      IPP_OP_VALIDATE_JOB,
  });

  // Ensure |ipp_oper_id| is an allowed operation.
  if (!kOperationIds.contains(ipp_oper_id)) {
    return ValidateAttributeResult::kFatalError;
  }

  // Ensure |name| refers to an allowed attribute.
  auto attr_def = kAttributeDefinitions.find(name);
  if (attr_def == kAttributeDefinitions.end()) {
    // TODO(crbug.com/945409): Tell caller to drop this attribute.
    return ValidateAttributeResult::kUnknownAttribute;
  }

  // Ensure every attribute has some value.
  if (values_count == 0) {
    return ValidateAttributeResult::kFatalError;
  }

  // Ensure single-valued attributes are single-valued.
  if (!attr_def->second.multivalued && values_count > 1) {
    return ValidateAttributeResult::kFatalError;
  }

  // Ensure every attribute was serialized to the correct type.
  if (attr_def->second.type != type) {
    return ValidateAttributeResult::kFatalError;
  }
  return ValidateAttributeResult::kSuccess;
}

}  // namespace cups_proxy
