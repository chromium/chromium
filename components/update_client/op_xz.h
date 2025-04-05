// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_OP_XZ_H_
#define COMPONENTS_UPDATE_CLIENT_OP_XZ_H_

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
class Unzipper;

// Decode the streams of an xz file. `callback` is posted to the sequence
// XzOperation was called on, with a file path containing the result of the
// decoding, if successful. If unsuccessful, `callback` is posted with an
// error. In either case, `in_file` is deleted. Returns a cancellation
// callback.
base::OnceClosure XzOperation(
    std::unique_ptr<Unzipper> unzipper,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    const base::FilePath& in_file,
    base::OnceCallback<void(base::expected<base::FilePath, CategorizedError>)>
        callback);

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_OP_XZ_H_
