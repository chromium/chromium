// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/proto_utils/google_rpc_code.h"

#include <map>
#include <string>

#include "base/no_destructor.h"
#include "base/strings/string_util.h"

namespace private_ai {

const char kGenericErrorPrefix[] = "generic::";

rpc::GoogleRpcCode ParseGoogleRpcCode(const std::string& reason) {
  // The reason string from the server may contain a canonical RPC error code.
  // This function attempts to parse it. The expected format is a string
  // containing "generic::<code>" with an optional ":<message>".
  // For example: "[ORIGINAL ERROR] generic::unavailable: Fail to do something"
  // From this string, "unavailable" is extracted and parsed. The prefixes and
  // message are ignored as they are not guaranteed to be present.
  size_t generic_start = reason.find(kGenericErrorPrefix);
  if (generic_start == std::string::npos) {
    return rpc::GoogleRpcCode::UNKNOWN;
  }

  size_t code_start = generic_start + sizeof(kGenericErrorPrefix) - 1;
  size_t end = reason.find(':', code_start);

  std::string code_str;
  if (end == std::string::npos) {
    code_str = base::ToUpperASCII(reason.substr(code_start));
  } else {
    code_str = base::ToUpperASCII(reason.substr(code_start, end - code_start));
  }

  rpc::GoogleRpcCode code;
  if (rpc::GoogleRpcCode_Parse(code_str, &code)) {
    return code;
  }
  return rpc::GoogleRpcCode::UNKNOWN;
}

}  // namespace private_ai
