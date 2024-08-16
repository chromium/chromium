// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/lobster/lobster_page_handler.h"

#include <string>

#include "ash/public/cpp/lobster/lobster_session.h"
#include "base/base64.h"

namespace ash {

LobsterPageHandler::LobsterPageHandler(LobsterSession* active_session)
    : session_(active_session) {}

LobsterPageHandler::~LobsterPageHandler() = default;

void LobsterPageHandler::DownloadCandidate(uint32_t candidate_id,
                                           DownloadCandidateCallback callback) {
  session_->DownloadCandidate(candidate_id, std::move(callback));
}

void LobsterPageHandler::CommitAsInsert(uint32_t candidate_id,
                                        CommitAsInsertCallback callback) {
  session_->CommitAsInsert(candidate_id, std::move(callback));
}

void LobsterPageHandler::CommitAsDownload(uint32_t candidate_id,
                                          CommitAsDownloadCallback callback) {
  session_->CommitAsDownload(candidate_id, std::move(callback));
}

void LobsterPageHandler::RequestCandidates(const std::string& query,
                                           uint32_t num_candidates,
                                           RequestCandidatesCallback callback) {
  session_->RequestCandidates(
      query, num_candidates,
      base::BindOnce(
          [](RequestCandidatesCallback callback, const LobsterResult& result) {
            if (!result.has_value()) {
              auto response =
                  lobster::mojom::Response::NewError(lobster::mojom::Error::New(
                      /*code=*/result.error().error_code,
                      /*message=*/result.error().message));

              std::move(callback).Run(std::move(response));
              return;
            }

            std::vector<lobster::mojom::CandidatePtr> candidates;
            for (const LobsterImageCandidate& candidate : *result) {
              candidates.push_back(lobster::mojom::Candidate::New(
                  /*id=*/candidate.id,
                  /*data_url=*/GURL(base::StrCat(
                      {"data:image/jpeg;base64,",
                       base::Base64Encode(candidate.image_bytes)}))));
            }
            std::move(callback).Run(
                lobster::mojom::Response::NewCandidates(std::move(candidates)));
          },
          std::move(callback)));
}

}  // namespace ash
