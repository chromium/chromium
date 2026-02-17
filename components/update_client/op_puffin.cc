// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/op_puffin.h"

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
#include "third_party/puffin/src/include/puffin/puffpatch.h"

namespace update_client {

base::OnceClosure PuffOperation(
    scoped_refptr<CrxCache> crx_cache,
    scoped_refptr<Patcher> patcher,
    base::RepeatingCallback<void(base::DictValue)> event_adder,
    base::RepeatingCallback<void(ComponentState)> state_tracker,
    const std::string& old_hash,
    const std::string& output_hash,
    const base::FilePath& patch_file,
    base::OnceCallback<void(base::expected<base::FilePath, CategorizedError>)>
        callback) {
  base::MakeRefCounted<DeltaPatchOperation>(
      crx_cache, event_adder, state_tracker, old_hash,
      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
          base::File::FLAG_WIN_EXCLUSIVE_WRITE,
      output_hash, puffin::P_OK, patch_file, protocol_request::kEventPuff,
      std::move(callback))
      ->Operation(base::BindOnce(&Patcher::PatchPuffPatch, patcher));
  return base::DoNothing();
}

}  // namespace update_client
