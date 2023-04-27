// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/change_owners_task.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

base::OnceCallback<void(base::OnceClosure)> MakeChangeOwnersTask(
    scoped_refptr<PersistedData> /* persisted_data */,
    UpdaterScope /* scope */) {
  return base::BindOnce([](base::OnceClosure callback) {
    // Do not change ownership on Windows.
    std::move(callback).Run();
  });
}

}  // namespace updater
