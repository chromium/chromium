// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/ipp_attribute_validator.h"

#include <cups/ipp.h>

#include <map>
#include <set>
#include <string>

#include "base/containers/contains.h"
#include "chrome/services/ipp_parser/public/mojom/ipp_parser.mojom.h"

namespace cups_proxy {

namespace {

using ValueType = ipp_parser::mojom::ValueType;

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
  static const std::map<std::string, AttributeDefinition> kAttributeDefinitions{
      {"attributes-charset", {false, ValueType::STRING}},
      {"attributes-natural-language", {false, ValueType::STRING}},
      {"auth-info", {true, ValueType::STRING}},
      {"auth-info-required", {true, ValueType::STRING}},
      {"charset-configured", {false, ValueType::STRING}},
      {"compression", {false, ValueType::STRING}},
      {"copies", {false, ValueType::INTEGER}},
      {"document-format", {false, ValueType::STRING}},
      {"document-name", {false, ValueType::STRING}},
      {"finishings", {true, ValueType::INTEGER}},
      {"ipp-attribute-fidelity", {false, ValueType::BOOLEAN}},
      {"job-cancel-after", {false, ValueType::INTEGER}},
      {"job-hold-until", {false, ValueType::STRING}},
      {"job-id", {false, ValueType::INTEGER}},
      {"job-k-limit", {false, ValueType::INTEGER}},
      {"job-media-progress", {false, ValueType::INTEGER}},
      {"job-name", {false, ValueType::STRING}},
      {"job-originating-host-name", {false, ValueType::STRING}},
      {"job-originating-user-name", {false, ValueType::STRING}},
      {"job-page-limit", {false, ValueType::INTEGER}},
      {"job-password", {false, ValueType::OCTET}},
      {"job-printer-state-message", {false, ValueType::STRING}},
      {"job-printer-state-reasons", {true, ValueType::STRING}},
      {"job-printer-up-time", {false, ValueType::INTEGER}},
      {"job-printer-uri", {false, ValueType::STRING}},
      {"job-priority", {false, ValueType::INTEGER}},
      {"job-quota-period", {false, ValueType::INTEGER}},
      {"job-state", {false, ValueType::INTEGER}},
      {"job-state-reasons", {true, ValueType::STRING}},
      {"job-uri", {false, ValueType::STRING}},
      {"last-document", {false, ValueType::BOOLEAN}},
      {"limit", {false, ValueType::INTEGER}},
      {"marker-change-time", {false, ValueType::INTEGER}},
      {"marker-colors", {true, ValueType::STRING}},
      {"marker-high-levels", {true, ValueType::INTEGER}},
      {"marker-levels", {true, ValueType::INTEGER}},
      {"marker-low-levels", {true, ValueType::INTEGER}},
      {"marker-message", {false, ValueType::STRING}},
      {"marker-names", {true, ValueType::STRING}},
      {"marker-types", {true, ValueType::STRING}},
      {"media", {false, ValueType::STRING}},
      {"media-ready", {true, ValueType::STRING}},
      // This is not a PWG attribute but CUPS supports it for InputSlot
      // selection.
      {"media-source", {true, ValueType::STRING}},
      {"member-names", {true, ValueType::STRING}},
      {"member-uris", {true, ValueType::STRING}},
      {"multiple-document-handling", {false, ValueType::STRING}},
      {"my-jobs", {false, ValueType::BOOLEAN}},
      {"natural-language-configured", {false, ValueType::STRING}},
      {"number-up", {false, ValueType::INTEGER}},
      {"orientation-requested", {false, ValueType::INTEGER}},
      {"output-bin", {false, ValueType::STRING}},
      {"page-border", {false, ValueType::STRING}},
      {"page-ranges", {true, ValueType::INTEGER}},
      {"pages-per-minute", {false, ValueType::INTEGER}},
      {"pages-per-minute-color", {false, ValueType::INTEGER}},
      {"print-color-mode", {false, ValueType::STRING}},
      {"print-quality", {false, ValueType::INTEGER}},
      {"printer-alert", {true, ValueType::STRING}},
      {"printer-alert-description", {true, ValueType::STRING}},
      {"printer-device-id", {false, ValueType::STRING}},
      {"printer-dns-sd-name", {false, ValueType::STRING}},
      {"printer-id", {false, ValueType::INTEGER}},
      {"printer-info", {false, ValueType::STRING}},
      {"printer-is-accepting-jobs", {false, ValueType::BOOLEAN}},
      {"printer-location", {false, ValueType::STRING}},
      {"printer-make-and-model", {false, ValueType::STRING}},
      {"printer-more-info", {false, ValueType::STRING}},
      {"printer-name", {false, ValueType::STRING}},
      {"printer-resolution", {false, ValueType::RESOLUTION}},
      {"printer-state", {false, ValueType::INTEGER}},
      {"printer-state-reasons", {true, ValueType::STRING}},
      {"printer-type", {false, ValueType::INTEGER}},
      {"printer-type-mask", {false, ValueType::INTEGER}},
      {"printer-up-time", {false, ValueType::INTEGER}},
      {"printer-uri", {false, ValueType::STRING}},
      {"queued-job-count", {false, ValueType::INTEGER}},
      {"requested-attributes", {true, ValueType::STRING}},
      {"requesting-user-name", {false, ValueType::STRING}},
      {"sides", {false, ValueType::STRING}},
      {"status-message", {false, ValueType::STRING}},
      {"time-at-completed", {false, ValueType::INTEGER}},
      {"time-at-creation", {false, ValueType::INTEGER}},
      {"time-at-processing", {false, ValueType::INTEGER}},
      {"which-jobs", {false, ValueType::STRING}}};

  // Allowed IPP Operations.
  static const std::set<ipp_op_t> kOperationIds{
      IPP_OP_CANCEL_JOB,         IPP_OP_CREATE_JOB,
      IPP_OP_CUPS_GET_DEFAULT,   IPP_OP_CUPS_GET_PPD,
      IPP_OP_CUPS_GET_PRINTERS,  IPP_OP_GET_JOBS,
      IPP_OP_GET_JOB_ATTRIBUTES, IPP_OP_GET_PRINTER_ATTRIBUTES,
      IPP_OP_HOLD_JOB,           IPP_OP_PAUSE_PRINTER,
      IPP_OP_PRINT_JOB,          IPP_OP_PRINT_URI,
      IPP_OP_RELEASE_JOB,        IPP_OP_RESUME_PRINTER,
      IPP_OP_SEND_DOCUMENT,      IPP_OP_SEND_URI,
      IPP_OP_VALIDATE_JOB,
  };

  // Ensure |ipp_oper_id| is an allowed operation.
  if (!base::Contains(kOperationIds, ipp_oper_id)) {
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
