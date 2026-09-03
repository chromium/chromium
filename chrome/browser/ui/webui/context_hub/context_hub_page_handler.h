// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONTEXT_HUB_CONTEXT_HUB_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CONTEXT_HUB_CONTEXT_HUB_PAGE_HANDLER_H_

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/uuid.h"
#include "chrome/browser/context_hub/context_hub_service.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace context_hub {
struct TabGroupEntry;
}  // namespace context_hub

class ContextHubPageHandler : public browser::context_hub::mojom::PageHandler,
                              public context_hub::ContextHubService::Observer {
 public:
  class TabProvider {
   public:
    virtual ~TabProvider() = default;
    virtual std::vector<content::WebContents*> GetTabs() = 0;
    virtual std::vector<content::WebContents*> GetUngroupedTabs() = 0;
    virtual void SwitchToTab(int64_t tab_id) = 0;
    virtual void CloseTab(int64_t tab_id) = 0;
    virtual bool ConfirmTabGroups(
        base::span<const context_hub::TabGroupEntry> groups) = 0;
    virtual void RemoveGroupFromTabstripIfOpen(
        const base::Uuid& saved_guid) = 0;
    virtual void UngroupGroupFromTabstripIfOpen(
        const base::Uuid& saved_guid) = 0;
  };

  ContextHubPageHandler(
      mojo::PendingRemote<browser::context_hub::mojom::Page> page,
      mojo::PendingReceiver<browser::context_hub::mojom::PageHandler> receiver,
      Profile* profile,
      content::WebContents* web_contents,
      std::unique_ptr<TabProvider> tab_provider = nullptr);
  ~ContextHubPageHandler() override;

  ContextHubPageHandler(const ContextHubPageHandler&) = delete;
  ContextHubPageHandler& operator=(const ContextHubPageHandler&) = delete;

  // context_hub::ContextHubService::Observer:
  void OnAutoTodosChanged(
      base::span<const context_hub::AutoTodoEntry> entries) override;
  void OnFirstPartyAutoTodosGenerationStateChanged(bool is_generating) override;
  void OnThirdPartyAutoTodosGenerationStateChanged(bool is_generating) override;

  // browser::context_hub::mojom::PageHandler:
  void GenerateFirstPartyAutoTodos(
      GenerateFirstPartyAutoTodosCallback callback) override;
  void GenerateTabBasedTodos(GenerateTabBasedTodosCallback callback) override;
  void GetAutoTodos(GetAutoTodosCallback callback) override;
  void UpdateAutoTodo(const context_hub::AutoTodoEntry& todo,
                      UpdateAutoTodoCallback callback) override;
  void ClearFirstPartyAutoTodos(
      ClearFirstPartyAutoTodosCallback callback) override;
  void ClearThirdPartyAutoTodos(
      ClearThirdPartyAutoTodosCallback callback) override;
  void SetTodoFeedback(
      browser::context_hub::mojom::AutoTodoItemFeedbackPtr feedback,
      SetTodoFeedbackCallback callback) override;
  void DeleteTodoFeedback(const std::string& id,
                          DeleteTodoFeedbackCallback callback) override;
  void ClearTodoFeedbacks(ClearTodoFeedbacksCallback callback) override;
  void GetTodoFeedbacks(GetTodoFeedbacksCallback callback) override;
  void GetSaveToMemoryBankContext(
      GetSaveToMemoryBankContextCallback callback) override;
  void GetAllMemoryBankEntries(
      GetAllMemoryBankEntriesCallback callback) override;
  void DeleteMemoryBankEntries(
      const std::vector<int64_t>& ids,
      DeleteMemoryBankEntriesCallback callback) override;
  void SaveMemoryBankEntry(
      browser::context_hub::mojom::MemoryBankEntryAnnotationsPtr annotations,
      SaveMemoryBankEntryCallback callback) override;
  void GetAllMemoryBankTags(GetAllMemoryBankTagsCallback callback) override;
  void GetAllMemoryBankCollections(
      GetAllMemoryBankCollectionsCallback callback) override;
  void UpdateMemoryBankEntryAnnotations(
      int64_t id,
      browser::context_hub::mojom::MemoryBankEntryAnnotationsPtr annotations,
      UpdateMemoryBankEntryAnnotationsCallback callback) override;
  void GetTabs(GetTabsCallback callback) override;
  void RetrieveAndGroupTabs(const std::string& user_command,
                            RetrieveAndGroupTabsCallback callback) override;
  void GetExistingTabGroupsAndChats(
      GetExistingTabGroupsAndChatsCallback callback) override;
  void SwitchToTab(int64_t tab_id) override;
  void CloseTab(int64_t tab_id) override;
  void ClearTabGroups(ClearTabGroupsCallback callback) override;
  void ClearTabGroupChatHistory(
      ClearTabGroupChatHistoryCallback callback) override;
  void AskGeminiWithContext(const std::string& user_command,
                            const std::vector<int64_t>& memory_bank_entry_ids,
                            AskGeminiWithContextCallback callback) override;
  void GetMemoryBankChatHistory(
      GetMemoryBankChatHistoryCallback callback) override;
  void ClearMemoryBankChatHistory(
      ClearMemoryBankChatHistoryCallback callback) override;
  void ConfirmAllTabGroups(ConfirmAllTabGroupsCallback callback) override;
  void GetConfirmedTabGroups(GetConfirmedTabGroupsCallback callback) override;
  void RemoveConfirmedTabGroup(
      const base::Uuid& saved_guid,
      RemoveConfirmedTabGroupCallback callback) override;
  void CloseConfirmedTabGroup(const base::Uuid& saved_guid,
                              CloseConfirmedTabGroupCallback callback) override;
  void RemoveAllConfirmedTabGroups(
      RemoveAllConfirmedTabGroupsCallback callback) override;

 private:
  mojo::Remote<browser::context_hub::mojom::Page> page_;
  mojo::Receiver<browser::context_hub::mojom::PageHandler> receiver_;
  base::ScopedObservation<context_hub::ContextHubService,
                          context_hub::ContextHubService::Observer>
      service_observation_{this};
  std::unique_ptr<TabProvider> tab_provider_;
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  base::WeakPtrFactory<ContextHubPageHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_CONTEXT_HUB_CONTEXT_HUB_PAGE_HANDLER_H_
