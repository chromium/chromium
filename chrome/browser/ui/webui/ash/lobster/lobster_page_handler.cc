// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/lobster/lobster_page_handler.h"

#include "ash/public/cpp/lobster/lobster_session.h"

namespace ash {

LobsterPageHandler::LobsterPageHandler(LobsterSession* active_session)
    : session_(active_session) {}

LobsterPageHandler::~LobsterPageHandler() = default;

void LobsterPageHandler::DownloadCandidate(int candidate_id,
                                           DownloadCandidateCallback callback) {
  session_->DownloadCandidate(candidate_id, std::move(callback));
}

}  // namespace ash
