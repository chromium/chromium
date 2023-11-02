// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/client_status.h"

#include "base/no_destructor.h"

namespace autofill_assistant {

ClientStatus::ClientStatus() : status_(UNKNOWN_ACTION_STATUS) {}
ClientStatus::ClientStatus(ProcessedActionStatusProto status)
    : status_(status) {}
ClientStatus::~ClientStatus() = default;

ClientStatus ClientStatus::WithStatusOverride(
    ProcessedActionStatusProto new_status) const {
  ClientStatus other = *this;
  if (status_ != new_status) {
    other.set_proto_status(new_status);
    other.mutable_details()->set_original_status(status_);
  }
  return other;
}

void ClientStatus::FillProto(ProcessedActionProto* proto) const {
  proto->set_status(status_);
  proto->set_slow_warning_status(slow_warning_status_);
  if (has_details_)
    proto->mutable_status_details()->MergeFrom(details_);
}

std::ostream& operator<<(std::ostream& out, const ClientStatus& status) {
  out << status.status_;
#ifndef NDEBUG
  if (status.details_.original_status() != UNKNOWN_ACTION_STATUS) {
    out << " was: " << status.details_.original_status();
  }
  if (status.details_.has_unexpected_error_info()) {
    auto& error_info = status.details_.unexpected_error_info();
    out << " " << error_info.source_file() << ":"
        << error_info.source_line_number();
    if (!error_info.js_exception_classname().empty()) {
      out << " JS error " << error_info.js_exception_classname();
      if (!error_info.js_exception_line_numbers().empty() &&
          !error_info.js_exception_column_numbers().empty()) {
        out << " at " << error_info.js_exception_line_numbers(0) << ":"
            << error_info.js_exception_column_numbers(0);
      }
    }
  }
#endif
  return out;
}

const ClientStatus& OkClientStatus() {
  static base::NoDestructor<ClientStatus> ok_(ACTION_APPLIED);
  return *ok_.get();
}

std::ostream& operator<<(std::ostream& out,
                         const ProcessedActionStatusProto& status) {
#ifdef NDEBUG
  out << static_cast<int>(status);
  return out;
#else
  switch (status) {
    case ProcessedActionStatusProto::UNKNOWN_ACTION_STATUS:
      out << "UNKNOWN_ACTION_STATUS";
      break;
    case ProcessedActionStatusProto::ELEMENT_RESOLUTION_FAILED:
      out << "ELEMENT_RESOLUTION_FAILED";
      break;
    case ProcessedActionStatusProto::ACTION_APPLIED:
      out << "ACTION_APPLIED";
      break;
    case ProcessedActionStatusProto::OTHER_ACTION_STATUS:
      out << "OTHER_ACTION_STATUS";
      break;
    case ProcessedActionStatusProto::COLLECT_USER_DATA_ERROR:
      out << "COLLECT_USER_DATA_ERROR";
      break;
    case ProcessedActionStatusProto::UNSUPPORTED_ACTION:
      out << "UNSUPPORTED_ACTION";
      break;
    case ProcessedActionStatusProto::MANUAL_FALLBACK:
      out << "MANUAL_FALLBACK";
      break;
    case ProcessedActionStatusProto::INTERRUPT_FAILED:
      out << "INTERRUPT_FAILED";
      break;
    case ProcessedActionStatusProto::USER_ABORTED_ACTION:
      out << "USER_ABORTED_ACTION";
      break;
    case ProcessedActionStatusProto::GET_FULL_CARD_FAILED:
      out << "GET_FULL_CARD_FAILED";
      break;
    case ProcessedActionStatusProto::PRECONDITION_FAILED:
      out << "PRECONDITION_FAILED";
      break;
    case ProcessedActionStatusProto::INVALID_ACTION:
      out << "INVALID_ACTION";
      break;
    case ProcessedActionStatusProto::UNSUPPORTED:
      out << "UNSUPPORTED";
      break;
    case ProcessedActionStatusProto::TIMED_OUT:
      out << "TIMED_OUT";
      break;
    case ProcessedActionStatusProto::ELEMENT_UNSTABLE:
      out << "ELEMENT_UNSTABLE";
      break;
    case ProcessedActionStatusProto::INVALID_SELECTOR:
      out << "INVALID_SELECTOR";
      break;
    case ProcessedActionStatusProto::OPTION_VALUE_NOT_FOUND:
      out << "OPTION_VALUE_NOT_FOUND";
      break;
    case ProcessedActionStatusProto::UNEXPECTED_JS_ERROR:
      out << "UNEXPECTED_JS_ERROR";
      break;
    case ProcessedActionStatusProto::TOO_MANY_ELEMENTS:
      out << "TOO_MANY_ELEMENTS";
      break;
    case ProcessedActionStatusProto::NAVIGATION_ERROR:
      out << "NAVIGATION_ERROR";
      break;
    case ProcessedActionStatusProto::AUTOFILL_INFO_NOT_AVAILABLE:
      out << "AUTOFILL_INFO_NOT_AVAILABLE";
      break;
    case ProcessedActionStatusProto::FRAME_HOST_NOT_FOUND:
      out << "FRAME_HOST_NOT_FOUND";
      break;
    case ProcessedActionStatusProto::AUTOFILL_INCOMPLETE:
      out << "AUTOFILL_INCOMPLETE";
      break;
    case ProcessedActionStatusProto::ELEMENT_MISMATCH:
      out << "ELEMENT_MISMATCH";
      break;
    case ProcessedActionStatusProto::ELEMENT_NOT_ON_TOP:
      out << "ELEMENT_NOT_ON_TOP";
      break;
    case ProcessedActionStatusProto::CLIENT_ID_RESOLUTION_FAILED:
      out << "CLIENT_ID_RESOLUTION_FAILED";
      break;
    case ProcessedActionStatusProto::PASSWORD_ORIGIN_MISMATCH:
      out << "PASSWORD_ORIGIN_MISMATCH";
      break;
    case ProcessedActionStatusProto::TOO_MANY_OPTION_VALUES_FOUND:
      out << "TOO_MANY_OPTION_VALUES_FOUND";
      break;
    case ProcessedActionStatusProto::INVALID_TARGET:
      out << "INVALID_TARGET";
      break;
    case ProcessedActionStatusProto::ELEMENT_POSITION_NOT_FOUND:
      out << "ELEMENT_POSITION_NOT_FOUND";
      break;
    case ProcessedActionStatusProto::CLIENT_MEMORY_KEY_NOT_AVAILABLE:
      out << "CLIENT_MEMORY_KEY_NOT_AVAILABLE";
      break;
    case ProcessedActionStatusProto::EMPTY_VALUE_EXPRESSION_RESULT:
      out << "EMPTY_VALUE_EXPRESSION_RESULT";
      break;
    case ProcessedActionStatusProto::NO_RENDER_FRAME:
      out << "NO_RENDER_FRAME";
      break;
    case ProcessedActionStatusProto::USER_DATA_REQUEST_FAILED:
      out << "USER_DATA_REQUEST_FAILED";
      break;
    case ProcessedActionStatusProto::JS_FORCED_ROUNDTRIP:
      out << "JS_FORCED_ROUNDTRIP";
      break;
    case ProcessedActionStatusProto::QR_CODE_SCAN_CANCELLED:
      out << "QR_CODE_SCAN_CANCELLED";
      break;
    case ProcessedActionStatusProto::QR_CODE_SCAN_FAILURE:
      out << "QR_CODE_SCAN_FAILURE";
      break;
    case ProcessedActionStatusProto::QR_CODE_SCAN_CAMERA_ERROR:
      out << "QR_CODE_SCAN_CAMERA_ERROR";
      break;
    case ProcessedActionStatusProto::XML_PARSE_INCORRECT_DATA:
      out << "XML_PARSE_INCORRECT_DATA";
      break;
    case ProcessedActionStatusProto::XML_PARSE_SIGNED_DATA:
      out << "XML_PARSE_SIGNED_DATA";
      break;

      // Intentionally no default case to make compilation fail if a new value
      // was added to the enum but not to this list.
  }
  return out;
#endif  // NDEBUG
}

}  // namespace autofill_assistant
