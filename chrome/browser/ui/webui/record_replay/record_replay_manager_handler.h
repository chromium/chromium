// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_RECORD_REPLAY_RECORD_REPLAY_MANAGER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_RECORD_REPLAY_RECORD_REPLAY_MANAGER_HANDLER_H_

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/record_replay/record_replay_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class AutocompleteController;
class Profile;

// Implementation of mojo::RecordReplayManagerHandler.  StartOmniboxQuery() calls to a
// private AutocompleteController. It also listens for updates from the
// AutocompleteController to OnResultChanged() and passes those results to
// the OmniboxPage.
class RecordReplayManagerHandler : public mojom::RecordReplayManagerHandler {
 public:
  // RecordReplayManagerHandler is deleted when the supplied pipe is destroyed.
  RecordReplayManagerHandler(Profile* profile,
                     mojo::PendingReceiver<mojom::RecordReplayManagerHandler> receiver);

  RecordReplayManagerHandler(const RecordReplayManagerHandler&) = delete;
  RecordReplayManagerHandler& operator=(const RecordReplayManagerHandler&) = delete;

  ~RecordReplayManagerHandler() override;

  // Called by RecordReplayUI to indicate that the login button was clicked.
  void HandleSignInButtonClicked();

  // mojom::RecordReplayManagerHandler overrides:
  void SetManager(mojo::PendingRemote<mojom::RecordReplayManager> manager) override;
  void GetEnv(const std::string& key, GetEnvCallback callback) override;
  void GetBuildId(GetBuildIdCallback callback) override;
  void GetReplayUserToken(GetReplayUserTokenCallback callback) override;
  void SetReplayUserToken(const absl::optional<std::string>& token) override;
  void GetReplayRefreshToken(GetReplayRefreshTokenCallback callback) override;
  void SetReplayRefreshToken(const absl::optional<std::string>& token) override;
  void ShowAuthenticationError(const std::string& message) override;
  void OpenExternalBrowser(const std::string& url) override;

 private:
  // Handle back to the page by which we can pass results.
  mojo::Remote<mojom::RecordReplayManager> manager_;

  // user and refresh tokens.
  absl::optional<std::string> record_replay_user_token_;
  absl::optional<std::string> record_replay_refresh_token_;

  // The Profile* handed to us in our constructor.
  raw_ptr<Profile> profile_;

  mojo::Receiver<mojom::RecordReplayManagerHandler> receiver_;

  base::WeakPtrFactory<RecordReplayManagerHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_RECORD_REPLAY_RECORD_REPLAY_MANAGER_HANDLER_H_
