// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/parse_manifest_from_manifest_url_command.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/web_applications/jobs/parse_manifest_from_string_job.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/web_contents/web_contents_manager.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app {

ParseManifestFromManifestUrlCommand::ParseManifestFromManifestUrlCommand(
    GURL manifest_url,
    std::string manifest_contents,
    ParseCallback callback)
    : WebAppCommand<SharedWebContentsLock, blink::mojom::ManifestPtr>(
          "ParseManifestFromManifestUrlCommand",
          SharedWebContentsLockDescription(),
          std::move(callback),
          /*args_for_shutdown=*/
          std::make_tuple(blink::mojom::ManifestPtr())),
      manifest_url_(std::move(manifest_url)),
      manifest_contents_(std::move(manifest_contents)) {
#if EXPENSIVE_DCHECKS_ARE_ON()
  GetMutableDebugValue().Set("manifest_contents", manifest_contents_);
#endif
  GetMutableDebugValue().Set("manifest_url", manifest_url_.spec());
}

ParseManifestFromManifestUrlCommand::~ParseManifestFromManifestUrlCommand() =
    default;

void ParseManifestFromManifestUrlCommand::StartWithLock(
    std::unique_ptr<SharedWebContentsLock> lock) {
  web_contents_lock_ = std::move(lock);

  // Use manifest_url_ as both document_url and manifest_url since there is no
  // associated document. This ensures relative URLs in the manifest resolve
  // correctly against the manifest's own origin.
  parse_job_ = std::make_unique<ParseManifestFromStringJob>(
      web_contents_lock_->web_contents_manager(),
      web_contents_lock_->shared_web_contents(),
      /*document_url=*/manifest_url_, /*manifest_url=*/manifest_url_,
      std::move(manifest_contents_),  // Consumed by parse_job_.
      *GetMutableDebugValue().EnsureDict("parse_manifest_from_string_job"),
      base::BindOnce(&ParseManifestFromManifestUrlCommand::OnJobComplete,
                     weak_ptr_factory_.GetWeakPtr()));
  parse_job_->Start();
}

void ParseManifestFromManifestUrlCommand::OnJobComplete(
    blink::mojom::ManifestPtr manifest) {
  parse_job_.reset();

  if (!manifest) {
    CompleteAndSelfDestruct(CommandResult::kFailure,
                            blink::mojom::ManifestPtr());
    return;
  }

  CompleteAndSelfDestruct(CommandResult::kSuccess, std::move(manifest));
}

}  // namespace web_app
