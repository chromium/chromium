// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_OP_PUFFIN_H_
#define COMPONENTS_UPDATE_CLIENT_OP_PUFFIN_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/update_client/crx_cache.h"
#include "components/update_client/patcher.h"

namespace base {
class FilePath;
}

namespace update_client {

struct CategorizedError;

// Apply a puffin patch. `callback` is posted to the sequence PuffOperation was
// called on, with a file path containing the result of the patch, if
// successful. If unsuccessful, `callback` is posted with an error. In either
// case, `patch_file` is deleted.
void PuffOperation(
    std::optional<scoped_refptr<CrxCache>> crx_cache,
    scoped_refptr<Patcher> patcher,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    const std::string& id,
    const std::string& prev_fp,
    const base::FilePath& patch_file,
    const base::FilePath& temp_dir,
    base::OnceCallback<void(
        const base::expected<base::FilePath, CategorizedError>&)> callback);

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_OP_PUFFIN_H_
