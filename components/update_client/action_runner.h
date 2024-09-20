// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_ACTION_RUNNER_H_
#define COMPONENTS_UPDATE_CLIENT_ACTION_RUNNER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "components/update_client/update_client.h"

namespace update_client {

void RunAction(scoped_refptr<ActionHandler> handler,
               scoped_refptr<CrxInstaller> installer,
               const std::string& file,
               const std::string& session_id,
               ActionHandler::Callback callback);

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_ACTION_RUNNER_H_
