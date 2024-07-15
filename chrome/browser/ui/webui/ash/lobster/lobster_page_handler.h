// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_LOBSTER_LOBSTER_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_LOBSTER_LOBSTER_PAGE_HANDLER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class LobsterSession;

class LobsterPageHandler {
 public:
  using DownloadCandidateCallback = base::OnceCallback<void(bool)>;

  explicit LobsterPageHandler(LobsterSession* active_session);

  ~LobsterPageHandler();

  void DownloadCandidate(int candidate_id, DownloadCandidateCallback);

 private:
  // Not owned by this class
  raw_ptr<LobsterSession> session_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_LOBSTER_LOBSTER_PAGE_HANDLER_H_
