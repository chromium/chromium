// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CRITICAL_ACTIONS_CRITICAL_ACTIONS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CRITICAL_ACTIONS_CRITICAL_ACTIONS_PAGE_HANDLER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/critical_actions/critical_actions.mojom.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace critical_actions {

// Maximum number of entries to query from the database for the internals page.
// Capping the query prevents unbounded table scans and memory usage if the
// database contains a very large number of rows.
inline constexpr size_t kMaxCriticalActionsQueryCount = 1000;

class CriticalActionsPageHandler : public mojom::PageHandler {
 public:
  CriticalActionsPageHandler(mojo::PendingReceiver<mojom::PageHandler> receiver,
                             Profile* profile);

  CriticalActionsPageHandler(const CriticalActionsPageHandler&) = delete;
  CriticalActionsPageHandler& operator=(const CriticalActionsPageHandler&) =
      delete;

  ~CriticalActionsPageHandler() override;

  // mojom::PageHandler:
  void GetCriticalActions(uint32_t page_index,
                          uint32_t page_size,
                          const std::optional<std::string>& search_query,
                          std::optional<int32_t> action_type_filter,
                          GetCriticalActionsCallback callback) override;

  void DeleteCriticalAction(const std::string& critical_action_id,
                            DeleteCriticalActionCallback callback) override;

  void ClearAllCriticalActions(
      ClearAllCriticalActionsCallback callback) override;

 private:
  void OnGetCriticalActionsComplete(uint32_t page_index,
                                    uint32_t page_size,
                                    std::optional<std::string> search_query,
                                    std::optional<int32_t> action_type_filter,
                                    GetCriticalActionsCallback callback,
                                    std::vector<CriticalActionEntry> entries);

  mojo::Receiver<mojom::PageHandler> receiver_;

  // The Profile associated with this WebUI. It is guaranteed to outlive this
  // handler because this handler is owned by CriticalActionsUI, which is tied
  // to the WebContents lifetime and destroyed before the Profile during
  // browser shutdown or profile teardown.
  const raw_ptr<Profile> profile_;

  base::WeakPtrFactory<CriticalActionsPageHandler> weak_ptr_factory_{this};
};

}  // namespace critical_actions

#endif  // CHROME_BROWSER_UI_WEBUI_CRITICAL_ACTIONS_CRITICAL_ACTIONS_PAGE_HANDLER_H_
