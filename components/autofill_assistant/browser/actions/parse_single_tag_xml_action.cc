// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/parse_single_tag_xml_action.h"

#include <vector>

#include "base/callback.h"
#include "base/numerics/safe_conversions.h"
#include "components/autofill_assistant/browser/actions/action_delegate.h"
#include "components/autofill_assistant/browser/user_data_util.h"
#include "components/autofill_assistant/browser/user_model.h"

namespace autofill_assistant {

ParseSingleTagXmlAction::ParseSingleTagXmlAction(ActionDelegate* delegate,
                                                 const ActionProto& proto)
    : Action(delegate, proto) {
  DCHECK(proto_.has_parse_single_tag_xml());
}

ParseSingleTagXmlAction::~ParseSingleTagXmlAction() = default;

void ParseSingleTagXmlAction::InternalProcessAction(
    ProcessActionCallback callback) {
  callback_ = std::move(callback);

  const std::string input_client_memory_key =
      proto_.parse_single_tag_xml().input_client_memory_key();
  if (input_client_memory_key.empty()) {
    DVLOG(2) << "ParseSingleTagXmlAction: Empty input client memory key "
                "not allowed.";
    EndAction(ClientStatus(INVALID_ACTION));
    return;
  }

  std::string input_xml_unsafe;
  // Get the XML string from the input client memory key of |UserModel|.
  auto status = user_data::GetClientMemoryStringValue(
      input_client_memory_key, delegate_->GetUserData(),
      delegate_->GetUserModel(), &input_xml_unsafe);
  if (!status.ok() || input_xml_unsafe.empty()) {
    DVLOG(2) << "ParseSingleTagXmlAction: Client memory doesn't contain a "
                "non-empty string value corresponding to the specified key.";
    EndAction(ClientStatus(PRECONDITION_FAILED));
    return;
  }

  if (proto_.parse_single_tag_xml().custom_failure_for_signed_input_data()) {
    // We check if given XML is signed or not and send appropriate client action
    // status back.
    // Since, the xml stored in |input_client_memory_key| is untrustworthy,
    // we should do all the data processing for that in Java.
    // This should not be done in C++ to follow the rule of 2.
    if (delegate_->IsXmlSigned(input_xml_unsafe)) {
      DVLOG(2) << "ParseSingleTagXmlAction: Signed XML.";
      EndAction(ClientStatus(XML_PARSE_SIGNED_DATA));
      return;
    }
  }

  auto xml_keys = std::vector<std::string>();
  // Verify that XML keys and output client memory keys are provided.
  for (const auto& field : proto_.parse_single_tag_xml().fields()) {
    if (field.key().empty() || field.output_client_memory_key().empty()) {
      DVLOG(2) << "ParseSingleTagXmlAction: Field is not defined properly.";
      EndAction(ClientStatus(INVALID_ACTION));
      return;
    }
    xml_keys.push_back(field.key());
  }

  const std::vector<std::string> output_values_unsafe =
      delegate_->ExtractValuesFromSingleTagXml(input_xml_unsafe, xml_keys);

  // Ensure that all values are read correctly and are non-empty strings.
  if (output_values_unsafe.size() !=
      base::checked_cast<size_t>(
          proto_.parse_single_tag_xml().fields().size())) {
    DVLOG(2) << "ParseSingleTagXmlAction: Failed to parse XML correctly.";
    EndAction(ClientStatus(XML_PARSE_INCORRECT_DATA));
    return;
  }
  const size_t output_size = output_values_unsafe.size();

  for (const std::string& output_value : output_values_unsafe) {
    if (output_value.empty()) {
      DVLOG(2) << "ParseSingleTagXmlAction: Certain values missing from XML.";
      EndAction(ClientStatus(XML_PARSE_INCORRECT_DATA));
      return;
    }
  }

  for (size_t i = 0; i < output_size; ++i) {
    // Store the output values in client memory.
    delegate_->GetUserModel()->SetValue(
        proto_.parse_single_tag_xml().fields().at(i).output_client_memory_key(),
        SimpleValue(output_values_unsafe.at(i),
                    /*  is_client_side_only= */ true));
  }

  EndAction(ClientStatus(ACTION_APPLIED));
}

void ParseSingleTagXmlAction::EndAction(const ClientStatus& status) {
  UpdateProcessedAction(status);
  std::move(callback_).Run(std::move(processed_action_proto_));
}

}  // namespace autofill_assistant
