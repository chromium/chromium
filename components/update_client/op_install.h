// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_OP_INSTALL_H_
#define COMPONENTS_UPDATE_CLIENT_OP_INSTALL_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/crx_cache.h"
#include "components/update_client/update_client.h"

namespace base {
class FilePath;
}

namespace update_client {

class Unzipper;

// Installs a CRX. `callback` is posted to the sequence InstallOperation was
// called on. If `crx_cache` is provided, `crx_file` is placed into the cache,
// regardless of whether the install is successful or not. Otherwise, `crx_file`
// is deleted.
void InstallOperation(
    std::optional<scoped_refptr<CrxCache>> crx_cache,
    std::unique_ptr<Unzipper> unzipper,
    crx_file::VerifierFormat crx_format,
    const std::string& id,
    const std::vector<uint8_t>& pk_hash,
    scoped_refptr<CrxInstaller> installer,
    std::unique_ptr<CrxInstaller::InstallParams> install_params,
    const std::string& next_fp,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    base::OnceCallback<void(const CrxInstaller::Result&)> callback,
    CrxInstaller::ProgressCallback progress_callback,
    const base::FilePath& crx_file);

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_OP_INSTALL_H_
