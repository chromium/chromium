// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_OP_ZUCCHINI_H_
#define COMPONENTS_UPDATE_CLIENT_OP_ZUCCHINI_H_

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

// Apply a zucchini patch. `callback` is posted to the sequence
// ZucchiniOperation was called on, with a file path containing the result of
// the patch, if successful. If unsuccessful, `callback` is posted with an
// error. In either case, `patch_file` is deleted. Returns a cancellation
// callback.
base::OnceClosure ZucchiniOperation(
    scoped_refptr<CrxCache> crx_cache,
    scoped_refptr<Patcher> patcher,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    const std::string& previous_hash,
    const std::string& output_hash,
    const base::FilePath& patch_file,
    base::OnceCallback<void(base::expected<base::FilePath, CategorizedError>)>
        callback);

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_OP_ZUCCHINI_H_
