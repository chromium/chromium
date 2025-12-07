// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_CLIENT_H_
#define COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_CLIENT_H_

#include <map>
#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/sessions_export.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/ui_base_types.h"

class GURL;

namespace gfx {
class Rect;
}

namespace tab_groups {
class TabGroupId;
}

namespace sessions {

class LiveTab;
class LiveTabContext;

// Callback from TabRestoreServiceClient::GetLastSession.
// The second parameter is the id of the window that was last active.
// The third parameter indicates if there was an error reading from the file.
using GetLastSessionCallback = base::OnceCallback<
    void(std::vector<std::unique_ptr<SessionWindow>>, SessionID, bool)>;

// A client interface that needs to be supplied to the tab restore service by
// the embedder.
class SESSIONS_EXPORT TabRestoreServiceClient {
 public:
  virtual ~TabRestoreServiceClient();

  // Creates a LiveTabContext instance that is associated with |app_name|. May
  // return nullptr (e.g., if the embedder does not support LiveTabContext
  // functionality).
  virtual LiveTabContext* CreateLiveTabContext(
      LiveTabContext* existing_context,
      SessionWindow::WindowType type,
      const std::string& app_name,
      const gfx::Rect& bounds,
      ui::mojom::WindowShowState show_state,
      const std::string& workspace,
      const std::string& user_title,
      const std::map<std::string, std::string>& extra_data) = 0;

  // Returns the LiveTabContext instance that is associated with
  // |tab|, or null if there is no such instance.
  virtual LiveTabContext* FindLiveTabContextForTab(const LiveTab* tab) = 0;

  // Returns the LiveTabContext instance that is associated with |desired_id|,
  // or null if there is no such instance.
  virtual LiveTabContext* FindLiveTabContextWithID(SessionID desired_id) = 0;

  // Returns the LiveTabContext instance that contains the group with ID
  // |group|, or null if there is no such instance.
  virtual LiveTabContext* FindLiveTabContextWithGroup(
      tab_groups::TabGroupId group) = 0;

  // Returns whether a given URL should be tracked for restoring.
  virtual bool ShouldTrackURLForRestore(const GURL& url) = 0;

  // Returns the extension app ID for the given LiveTab, or the empty string
  // if there is no such ID (e.g., if extensions are not supported by the
  // embedder).
  virtual std::string GetExtensionAppIDForTab(LiveTab* tab) = 0;

  // Returns the path of the directory to save state into.
  virtual base::FilePath GetPathToSaveTo() = 0;

  // Returns the URL that corresponds to the new tab page.
  virtual GURL GetNewTabURL() = 0;

  // Returns whether there is a previous session to load.
  virtual bool HasLastSession() = 0;

  // Fetches the contents of the last session, notifying the callback when
  // done. If the callback is supplied an empty vector of SessionWindows
  // it means the session could not be restored.
  virtual void GetLastSession(GetLastSessionCallback callback) = 0;

  // Called when a tab is restored. |url| is the URL that the tab is currently
  // visiting.
  virtual void OnTabRestored(const GURL& url);
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CORE_TAB_RESTORE_SERVICE_CLIENT_H_
