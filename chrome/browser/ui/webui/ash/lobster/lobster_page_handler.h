// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOBSTER_LOBSTER_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOBSTER_LOBSTER_PAGE_HANDLER_H_

#include <string>

#include "ash/public/cpp/lobster/lobster_result.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/lobster/lobster.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash {

class LobsterSession;

class LobsterPageHandler : public lobster::mojom::UntrustedLobsterPageHandler {
 public:
  explicit LobsterPageHandler(LobsterSession* active_session, Profile* profile);

  ~LobsterPageHandler() override;

  void BindInterface(
      mojo::PendingReceiver<lobster::mojom::UntrustedLobsterPageHandler>
          pending_receiver);

  // lobster::mojom::LobsterPageHandler overrides
  void RequestCandidates(const std::string& query,
                         uint32_t num_candidates,
                         RequestCandidatesCallback) override;
  void DownloadCandidate(uint32_t candidate_id,
                         DownloadCandidateCallback) override;
  void CommitAsInsert(uint32_t candidate_id, CommitAsInsertCallback) override;
  void CommitAsDownload(uint32_t candidate_id,
                        CommitAsDownloadCallback) override;
  void PreviewFeedback(uint32_t candidate_id, PreviewFeedbackCallback) override;
  void SubmitFeedback(uint32_t candidate_id,
                      const std::string& description,
                      SubmitFeedbackCallback) override;
  void ShowUI() override;
  void CloseUI() override;

 private:
  // Not owned by this class
  raw_ptr<LobsterSession> session_;
  raw_ptr<Profile> profile_;

  mojo::Receiver<lobster::mojom::UntrustedLobsterPageHandler> receiver_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOBSTER_LOBSTER_PAGE_HANDLER_H_
