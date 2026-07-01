// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_OP_SPACE_CHECK_H_
#define COMPONENTS_UPDATE_CLIENT_OP_SPACE_CHECK_H_

#include <cstdint>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/update_client/protocol_parser.h"
#include "components/update_client/update_client_errors.h"

namespace update_client {

// SpaceCheckOperation estimates whether there is enough available disk space to
// download and install the update. If the space is insufficient, the operation
// posts `callback` with a `DISK_FULL` downloader error. Otherwise, it
// propagates the input `path` to `callback`. Cancellation is not supported in
// this operation, a no-op callback is returned for API compatibility.
base::OnceClosure SpaceCheckOperation(
    const ProtocolParser::Pipeline& pipeline,
    base::RepeatingCallback<int64_t(const base::FilePath&)> get_available_space,
    base::RepeatingCallback<void(base::DictValue)> event_adder,
    const base::FilePath& path,
    base::OnceCallback<void(base::expected<base::FilePath, CategorizedError>)>
        callback);

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_OP_SPACE_CHECK_H_
