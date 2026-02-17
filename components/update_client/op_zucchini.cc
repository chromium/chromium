// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/op_zucchini.h"

#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/update_client/crx_cache.h"
#include "components/update_client/delta_patch_operation.h"
#include "components/update_client/patcher.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"

namespace update_client {

base::OnceClosure ZucchiniOperation(
    scoped_refptr<CrxCache> crx_cache,
    scoped_refptr<Patcher> patcher,
    base::RepeatingCallback<void(base::DictValue)> event_adder,
    base::RepeatingCallback<void(ComponentState)> state_tracker,
    const std::string& previous_hash,
    const std::string& output_hash,
    const base::FilePath& patch_file,
    base::OnceCallback<void(base::expected<base::FilePath, CategorizedError>)>
        callback) {
  base::MakeRefCounted<DeltaPatchOperation>(
      crx_cache, event_adder, state_tracker, previous_hash,
      base::File::FLAG_CREATE | base::File::FLAG_READ | base::File::FLAG_WRITE |
          base::File::FLAG_WIN_EXCLUSIVE_WRITE |
          base::File::FLAG_WIN_SHARE_DELETE |
          base::File::FLAG_CAN_DELETE_ON_CLOSE,
      output_hash, 0, patch_file, protocol_request::kEventZucchini,
      std::move(callback))
      ->Operation(base::BindOnce(&Patcher::PatchZucchini, patcher));
  return base::DoNothing();
}

}  // namespace update_client
