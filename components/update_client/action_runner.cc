// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/action_runner.h"

#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/update_client/protocol_definition.h"

namespace update_client {

namespace {

base::Value::Dict MakeEventActionRun(bool succeeded,
                                     int error_code,
                                     int extra_code1) {
  base::Value::Dict event;
  event.Set("eventtype", protocol_request::kEventAction);
  event.Set("eventresult", static_cast<int>(succeeded));
  if (error_code) {
    event.Set("errorcode", error_code);
  }
  if (extra_code1) {
    event.Set("extracode1", extra_code1);
  }
  return event;
}

}  // namespace

base::OnceClosure RunAction(
    scoped_refptr<ActionHandler> handler,
    scoped_refptr<CrxInstaller> installer,
    const std::string& file,
    const std::string& session_id,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    ActionHandler::Callback callback) {
  if (!handler) {
    DVLOG(1) << file << " is missing an action handler";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false, -1, 0));
    return base::DoNothing();
  }

  std::optional<base::FilePath> crx_path = installer->GetInstalledFile(file);
  if (!crx_path) {
    DVLOG(1) << file << " file is missing.";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false, -1, 0));
    return base::DoNothing();
  }
  handler->Handle(
      *crx_path, session_id,
      base::BindOnce(
          [](ActionHandler::Callback callback,
             base::RepeatingCallback<void(base::Value::Dict)> event_adder,
             bool succeeded, int error_code, int extra_code1) {
            event_adder.Run(
                MakeEventActionRun(succeeded, error_code, extra_code1));
            std::move(callback).Run(succeeded, error_code, extra_code1);
          },
          std::move(callback), event_adder));
  return base::DoNothing();
}

}  // namespace update_client
