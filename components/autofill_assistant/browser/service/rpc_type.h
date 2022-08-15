// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_RPC_TYPE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_RPC_TYPE_H_

namespace autofill_assistant {

enum class RpcType {
  UNKNOWN,
  GET_ACTIONS,
  GET_TRIGGER_SCRIPTS,
  SUPPORTS_SCRIPT,
  GET_CAPABILITIES_BY_HASH_PREFIX,
  GET_USER_DATA,
  REPORT_PROGRESS,
};
}

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_RPC_TYPE_H_
