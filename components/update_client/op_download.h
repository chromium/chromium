// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_OP_DOWNLOAD_H_
#define COMPONENTS_UPDATE_CLIENT_OP_DOWNLOAD_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/update_engine.h"

namespace base {
class FilePath;
}

namespace update_client {

struct CategorizedError;

// Do a download operation. Returns a cancellation callback. Once the download
// is complete, it will invoke `callback` on the same sequence it was started
// on, with a file path containing the result of the download, if successful.
// If unsuccessful or cancelled, `callback` will be invoked with an error. The
// cancellation callback can only be invoked on the same sequence the operation
// is started on.
base::OnceClosure DownloadOperation(
    scoped_refptr<const UpdateContext> update_context,
    const std::vector<GURL>& urls,
    int64_t size,
    const std::string& hash,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    CrxDownloader::ProgressCallback progress_callback,
    base::OnceCallback<void(
        const base::expected<base::FilePath, CategorizedError>&)> callback);

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_OP_DOWNLOAD_H_
