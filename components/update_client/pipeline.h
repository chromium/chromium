// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PIPELINE_H_
#define COMPONENTS_UPDATE_CLIENT_PIPELINE_H_

#include <cstdint>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/protocol_parser.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"

namespace base {
class FilePath;
}

namespace update_client {

class Configurator;
class CrxCache;

using PipelineStartCallback = base::OnceCallback<base::OnceClosure(
    base::OnceCallback<void(const CategorizedError&)>)>;

// Creates a pipeline of operations to update an item, and returns a callback
// that starts the pipeline as a parameter of `callback`. The pipeline callback
// in turn takes as an argument a completion callback to invoke when done, and
// returns a cancellation callback. The completion callback will be invoked on
// the same sequence that the pipeline is started on, and the cancellation
// callback must only be called on that same sequence. If `result` indicates a
// server error, returns instead a CategorizedError with
// ErrorCategory::kUpdateCheck. If `result` indicates no-update, returns
// instead a CategorizedError with ErrorCategory::kNone. As the pipeline
// executes operations, it calls `state_tracker` to record the ComponentState
// to expose to observers. It also calls `event_adder` to record events to be
// sent to the update server, and `diff_outcome_recorder` to record the outcome
// of the differential update (if any).
void MakePipeline(
    scoped_refptr<Configurator> config,
    base::RepeatingCallback<int64_t(const base::FilePath&)> get_available_space,
    bool is_foreground,
    const std::string& session_id,
    scoped_refptr<CrxCache> crx_cache,
    crx_file::VerifierFormat crx_format,
    const std::string& id,
    const std::vector<uint8_t>& pk_hash,
    const std::string& install_data_index,
    const std::string& prev_fp,
    scoped_refptr<CrxInstaller> installer,
    base::RepeatingCallback<void(ComponentState)> state_tracker,
    base::RepeatingCallback<void(base::Value::Dict)> event_adder,
    CrxDownloader::ProgressCallback download_progress_callback,
    CrxInstaller::ProgressCallback install_progress_callback,
    base::RepeatingCallback<void(const CrxInstaller::Result&)>
        install_complete_callback,
    scoped_refptr<ActionHandler> action_handler,
    base::RepeatingCallback<void(const CategorizedError&)>
        diff_outcome_recorder,
    const ProtocolParser::Result& result,
    base::OnceCallback<void(
        base::expected<PipelineStartCallback, CategorizedError>)> callback);

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PIPELINE_H_
