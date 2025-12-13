// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_OUT_OF_PROCESS_PATCHER_H_
#define CHROME_UPDATER_OUT_OF_PROCESS_PATCHER_H_

#include "base/memory/scoped_refptr.h"
#include "chrome/updater/updater_scope.h"
#include "components/update_client/patcher.h"

namespace updater {

// Creates an out-of-process patcher.
class OutOfProcessPatcherFactory : public update_client::PatcherFactory {
 public:
  explicit OutOfProcessPatcherFactory(UpdaterScope scope);

  // Overrides for update_client::PatcherFactory.
  scoped_refptr<update_client::Patcher> Create() const override;

 protected:
  ~OutOfProcessPatcherFactory() override = default;

 private:
  UpdaterScope scope_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_OUT_OF_PROCESS_PATCHER_H_
