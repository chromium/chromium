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
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"

namespace update_client {

void RunAction(scoped_refptr<ActionHandler> handler,
               scoped_refptr<CrxInstaller> installer,
               const std::string& file,
               const std::string& session_id,
               ActionHandler::Callback callback) {
  if (!handler) {
    DVLOG(1) << file << " is missing an action handler";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false, -1, 0));
    return;
  }

  std::optional<base::FilePath> crx_path = installer->GetInstalledFile(file);
  if (!crx_path) {
    DVLOG(1) << file << " file is missing.";
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false, -1, 0));
    return;
  }
  handler->Handle(*crx_path, session_id, std::move(callback));
}

}  // namespace update_client
