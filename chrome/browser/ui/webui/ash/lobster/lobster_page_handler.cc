// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/lobster/lobster_page_handler.h"

#include <string>

#include "ash/public/cpp/lobster/lobster_session.h"
#include "base/base64.h"
#include "base/strings/strcat.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"

namespace ash {

base::FilePath GetDownloadDirectoryForProfile(Profile* profile) {
  return DownloadPrefs::FromBrowserContext(profile)
      ->GetDefaultDownloadDirectoryForProfile();
}

LobsterPageHandler::LobsterPageHandler(LobsterSession* active_session,
                                       Profile* profile)
    : session_(active_session), profile_(profile) {}

LobsterPageHandler::~LobsterPageHandler() = default;

void LobsterPageHandler::BindInterface(
    mojo::PendingReceiver<lobster::mojom::UntrustedLobsterPageHandler>
        pending_receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(pending_receiver));
}

void LobsterPageHandler::DownloadCandidate(uint32_t candidate_id,
                                           DownloadCandidateCallback callback) {
  // TODO: b:359361699 - Implements smarter file naming
  session_->DownloadCandidate(
      candidate_id,
      GetDownloadDirectoryForProfile(profile_).Append("sample.jpeg"),
      std::move(callback));
}

void LobsterPageHandler::CommitAsInsert(uint32_t candidate_id,
                                        CommitAsInsertCallback callback) {
  session_->CommitAsInsert(candidate_id, std::move(callback));
}

void LobsterPageHandler::CommitAsDownload(uint32_t candidate_id,
                                          CommitAsDownloadCallback callback) {
  // TODO: b:359361699 - Implements smarter file naming
  session_->CommitAsDownload(
      candidate_id,
      GetDownloadDirectoryForProfile(profile_).Append("sample.jpeg"),
      std::move(callback));
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

void LobsterPageHandler::PreviewFeedback(uint32_t candidate_id,
                                         PreviewFeedbackCallback callback) {
  session_->PreviewFeedback(
      candidate_id,
      base::BindOnce(
          [](PreviewFeedbackCallback callback,
             const LobsterFeedbackPreviewResponse& response) {
            if (response.has_value()) {
              std::move(callback).Run(std::move(response.value()));
            }
          },
          std::move(callback)));
}

void LobsterPageHandler::SubmitFeedback(uint32_t candidate_id,
                                        const std::string& description,
                                        SubmitFeedbackCallback callback) {
  std::move(callback).Run(
      /*success=*/session_->SubmitFeedback(candidate_id, description));
}

void LobsterPageHandler::ShowUI() {
  session_->ShowUI();
}

void LobsterPageHandler::CloseUI() {
  session_->CloseUI();
}

}  // namespace ash
