// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_PARSE_SINGLE_TAG_XML_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_PARSE_SINGLE_TAG_XML_ACTION_H_

#include "components/autofill_assistant/browser/actions/action.h"

namespace autofill_assistant {

// Reads XML from a |input_client_memory_key| and extracts the set of keys in
// the |output_client_memory_key|.
//
// For the following XML stored at "xml_client_memory_key":
//
// <?xml version='1.0'  encoding='UTF-8'?>
// <PersonData id='1234' />
//
// the PersonData id can be extracted into |output_client_memory_key|
// "person_id_client_memory_key" by the below action proto:
//
// ParseSingleTagXml {
//  input_client_memory_key: "xml_client_memory_key"
//  field {
//      key: "id"
//      output_client_memory_key: "person_id_client_memory_key"
//  }
// }
//
//  Then "person_id_client_memory_key" will contain "1234".
class ParseSingleTagXmlAction : public Action {
 public:
  explicit ParseSingleTagXmlAction(ActionDelegate* delegate,
                                   const ActionProto& proto);

  ParseSingleTagXmlAction(const ParseSingleTagXmlAction&) = delete;
  ParseSingleTagXmlAction& operator=(const ParseSingleTagXmlAction&) = delete;

  ~ParseSingleTagXmlAction() override;

 private:
  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void EndAction(const ClientStatus& status);

  ProcessActionCallback callback_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_PARSE_SINGLE_TAG_XML_ACTION_H_
