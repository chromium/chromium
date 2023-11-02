// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_FUZZER_FORM_DATA_PROTO_PRODUCER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_FUZZER_FORM_DATA_PROTO_PRODUCER_H_

#include "components/autofill/core/common/form_data.h"

namespace form_data_fuzzer {
class Form;
}

namespace password_manager {

// Generates a |FormData| object based on values represented by a parsed
// protobuf |form_proto|.
autofill::FormData GenerateWithProto(
    const ::form_data_fuzzer::Form& form_proto);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FORM_PARSING_FUZZER_FORM_DATA_PROTO_PRODUCER_H_
