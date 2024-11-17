// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_ACTION_RUNNER_H_
#define COMPONENTS_UPDATE_CLIENT_ACTION_RUNNER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "components/update_client/update_client.h"

namespace update_client {

// Runs an action. Returns a cancellation callback.
base::OnceClosure RunAction(
    scoped_refptr<ActionHandler> handler,
    scoped_refptr<CrxInstaller> installer,
    const std::string& file,
    const std::string& session_id,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    ActionHandler::Callback callback);

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_ACTION_RUNNER_H_
