// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONTEXT_HUB_CONTEXT_HUB_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CONTEXT_HUB_CONTEXT_HUB_PAGE_HANDLER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom.h"
#include "components/personal_context/proto/features/auto_todos.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

class ContextHubPageHandler : public browser::context_hub::mojom::PageHandler {
 public:
  class TabProvider {
   public:
    virtual ~TabProvider() = default;
    virtual std::vector<content::WebContents*> GetTabs(
        content::WebContents* web_contents) = 0;
    virtual void SwitchToTab(content::WebContents* web_contents,
                             int64_t tab_id) = 0;
  };

  ContextHubPageHandler(
      mojo::PendingReceiver<browser::context_hub::mojom::PageHandler> receiver,
      Profile* profile,
      content::WebContents* web_contents,
      std::unique_ptr<TabProvider> tab_provider = nullptr);
  ~ContextHubPageHandler() override;

  ContextHubPageHandler(const ContextHubPageHandler&) = delete;
  ContextHubPageHandler& operator=(const ContextHubPageHandler&) = delete;

  // browser::context_hub::mojom::PageHandler:
  void GenerateAutoTodos(GenerateAutoTodosCallback callback) override;
  void GetAllMemoryBankEntries(
      GetAllMemoryBankEntriesCallback callback) override;
  void DeleteMemoryBankEntries(
      const std::vector<int64_t>& ids,
      DeleteMemoryBankEntriesCallback callback) override;
  void GetTabs(GetTabsCallback callback) override;
  void RetrieveAndGroupTabs(const std::string& user_command,
                            RetrieveAndGroupTabsCallback callback) override;
  void GetExistingTabGroupsAndChats(
      GetExistingTabGroupsAndChatsCallback callback) override;
  void SwitchToTab(int64_t tab_id) override;
  void ClearTabGroups(ClearTabGroupsCallback callback) override;
  void ClearTabGroupChatHistory(
      ClearTabGroupChatHistoryCallback callback) override;
  void AskGeminiWithContext(const std::string& user_command,
                            const std::vector<int64_t>& memory_bank_entry_ids,
                            AskGeminiWithContextCallback callback) override;

 private:
  void OnAutoTodosGenerated(
      GenerateAutoTodosCallback callback,
      std::optional<personal_context::proto::AutoTodosResponse> result);

  mojo::Receiver<browser::context_hub::mojom::PageHandler> receiver_;
  std::unique_ptr<TabProvider> tab_provider_;
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  base::WeakPtrFactory<ContextHubPageHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_CONTEXT_HUB_CONTEXT_HUB_PAGE_HANDLER_H_
