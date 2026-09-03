// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/context_hub/context_hub_page_handler.h"

#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "chrome/browser/context_hub/auto_todos/auto_todo_entry.h"
#include "chrome/browser/context_hub/context_hub_service.h"
#include "chrome/browser/context_hub/context_hub_service_factory.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/context_hub/context_hub_tab_provider_desktop.h"
#endif
#include "components/sessions/content/session_tab_helper.h"  // nogncheck

ContextHubPageHandler::ContextHubPageHandler(
    mojo::PendingRemote<browser::context_hub::mojom::Page> page,
    mojo::PendingReceiver<browser::context_hub::mojom::PageHandler> receiver,
    Profile* profile,
    content::WebContents* web_contents,
    std::unique_ptr<TabProvider> tab_provider)
    : page_(std::move(page)),
      receiver_(this, std::move(receiver)),
      tab_provider_(std::move(tab_provider)),
      profile_(profile),
      web_contents_(web_contents) {
  CHECK(page_.is_bound());
  if (!tab_provider_) {
#if !BUILDFLAG(IS_ANDROID)
    tab_provider_ =
        std::make_unique<context_hub::ContextHubTabProviderDesktop>(profile_);
#endif
  }
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    service_observation_.Observe(service);
    if (service->IsGeneratingFirstPartyAutoTodos()) {
      page_->OnFirstPartyAutoTodosGenerationStateChanged(true);
    }
  }
}

ContextHubPageHandler::~ContextHubPageHandler() = default;

void ContextHubPageHandler::OnAutoTodosChanged(
    base::span<const context_hub::AutoTodoEntry> entries) {
  std::vector<context_hub::AutoTodoEntry> visible_entries;
  for (const auto& entry : entries) {
    // TODO(crbug.com/540562062): Consider showing dismissed todos in a separate
    // section.
    if (entry.status == context_hub::AutoTodoEntry::Status::kDismissed) {
      continue;
    }
    visible_entries.push_back(entry);
  }
  page_->OnAutoTodosChanged(std::move(visible_entries));
}

void ContextHubPageHandler::OnFirstPartyAutoTodosGenerationStateChanged(
    bool is_generating) {
  page_->OnFirstPartyAutoTodosGenerationStateChanged(is_generating);
}

void ContextHubPageHandler::OnThirdPartyAutoTodosGenerationStateChanged(
    bool is_generating) {
  page_->OnThirdPartyAutoTodosGenerationStateChanged(is_generating);
}

void ContextHubPageHandler::GenerateFirstPartyAutoTodos(
    GenerateFirstPartyAutoTodosCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run(false);
    return;
  }

  service->GenerateFirstPartyAutoTodos(std::move(callback));
}

void ContextHubPageHandler::GetAutoTodos(GetAutoTodosCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run({}, {}, base::Time(), base::Time());
    return;
  }

  base::Time last_first_party_generation_time =
      service->GetLastFirstPartyGenerationTime();
  base::Time last_third_party_generation_time =
      service->GetLastThirdPartyGenerationTime();

  service->GetAutoTodos(base::BindOnce(
      [](GetAutoTodosCallback callback,
         base::Time last_first_party_generation_time,
         base::Time last_third_party_generation_time,
         std::vector<context_hub::AutoTodoEntry> entries) {
        std::vector<context_hub::AutoTodoEntry> first_party_todos;
        std::vector<context_hub::AutoTodoEntry> third_party_todos;
        for (auto& entry : entries) {
          if (entry.status == context_hub::AutoTodoEntry::Status::kDismissed) {
            continue;
          }
          if (entry.is_first_party()) {
            first_party_todos.push_back(std::move(entry));
          } else if (entry.is_third_party()) {
            third_party_todos.push_back(std::move(entry));
          }
        }
        std::move(callback).Run(
            std::move(first_party_todos), std::move(third_party_todos),
            last_first_party_generation_time, last_third_party_generation_time);
      },
      std::move(callback), last_first_party_generation_time,
      last_third_party_generation_time));
}

void ContextHubPageHandler::UpdateAutoTodo(
    const context_hub::AutoTodoEntry& todo,
    UpdateAutoTodoCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run(false);
    return;
  }

  service->UpdateAutoTodo(todo, std::move(callback));
}

void ContextHubPageHandler::ClearFirstPartyAutoTodos(
    ClearFirstPartyAutoTodosCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    service->ClearFirstPartyAutoTodos(std::move(callback));
    return;
  }
  std::move(callback).Run(false);
}

void ContextHubPageHandler::ClearThirdPartyAutoTodos(
    ClearThirdPartyAutoTodosCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    service->ClearThirdPartyAutoTodos(std::move(callback));
    return;
  }
  std::move(callback).Run(false);
}

void ContextHubPageHandler::SetTodoFeedback(
    browser::context_hub::mojom::AutoTodoItemFeedbackPtr feedback,
    SetTodoFeedbackCallback callback) {
  CHECK(feedback);
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    service->SetTodoFeedback(std::move(feedback));
  }
  std::move(callback).Run();
}

void ContextHubPageHandler::DeleteTodoFeedback(
    const std::string& id,
    DeleteTodoFeedbackCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    service->DeleteTodoFeedback(id);
  }
  std::move(callback).Run();
}

void ContextHubPageHandler::ClearTodoFeedbacks(
    ClearTodoFeedbacksCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    service->ClearTodoFeedbacks();
  }
  std::move(callback).Run();
}

void ContextHubPageHandler::GetTodoFeedbacks(
    GetTodoFeedbacksCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    std::move(callback).Run(service->GetTodoFeedbacks());
    return;
  }
  std::move(callback).Run({});
}

void ContextHubPageHandler::GetSaveToMemoryBankContext(
    GetSaveToMemoryBankContextCallback callback) {
  auto* service = ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    if (auto pending = service->GetPendingMemoryBankEntry()) {
      auto mojo_context =
          browser::context_hub::mojom::SaveToMemoryBankContext::New();
      mojo_context->url = pending->url;
      mojo_context->tab_title = pending->tab_title;
      bool is_text_selection =
          pending->type == context_hub::MemoryBankType::kTextSelection;
      if (is_text_selection && pending->selected_text.has_value()) {
        // Truncate the snippet to a reasonable length since we only need a
        // preview in the UI.
        static constexpr size_t kMaxSnippetPreviewLength = 300;
        std::string preview = *pending->selected_text;
        if (preview.length() > kMaxSnippetPreviewLength) {
          preview.resize(kMaxSnippetPreviewLength);
        }
        mojo_context->selected_text = std::move(preview);
      }
      mojo_context->is_text_selection = is_text_selection;
      std::move(callback).Run(std::move(mojo_context));
      return;
    }
  }
  std::move(callback).Run(nullptr);
}

void ContextHubPageHandler::GetAllMemoryBankEntries(
    GetAllMemoryBankEntriesCallback callback) {
  auto* service = ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run({});
    return;
  }

  service->GetAllEntries(base::BindOnce(
      [](GetAllMemoryBankEntriesCallback callback,
         std::vector<context_hub::MemoryBankEntry> entries) {
        std::vector<browser::context_hub::mojom::MemoryBankEntryPtr>
            mojo_entries;
        for (const auto& entry : entries) {
          auto mojo_entry = browser::context_hub::mojom::MemoryBankEntry::New();
          mojo_entry->id = entry.id;
          switch (entry.type) {
            case context_hub::MemoryBankType::kTab:
              mojo_entry->type = browser::context_hub::mojom::EntryType::kTab;
              break;
            case context_hub::MemoryBankType::kTextSelection:
              mojo_entry->type =
                  browser::context_hub::mojom::EntryType::kTextSelection;
              break;
          }
          mojo_entry->timestamp = entry.timestamp;
          mojo_entry->url = entry.url;
          mojo_entry->tab_title = entry.tab_title;
          mojo_entry->selected_text = entry.selected_text;
          mojo_entry->tags = entry.tags;
          mojo_entry->note = entry.note;
          mojo_entry->collection = entry.collection;
          mojo_entries.push_back(std::move(mojo_entry));
        }
        std::move(callback).Run(std::move(mojo_entries));
      },
      std::move(callback)));
}

void ContextHubPageHandler::DeleteMemoryBankEntries(
    const std::vector<int64_t>& ids,
    DeleteMemoryBankEntriesCallback callback) {
  auto* service = ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run();
    return;
  }

  service->DeleteEntries(ids, base::IgnoreArgs<bool>(std::move(callback)));
}

void ContextHubPageHandler::SaveMemoryBankEntry(
    browser::context_hub::mojom::MemoryBankEntryAnnotationsPtr annotations,
    SaveMemoryBankEntryCallback callback) {
  auto* service = ContextHubServiceFactory::GetForProfile(profile_);
  if (service && annotations) {
    std::vector<std::string> tags =
        std::move(annotations->tags).value_or(std::vector<std::string>{});
    bool success = service->SavePendingMemoryBankEntry(
        std::move(tags), std::move(annotations->note),
        std::move(annotations->collection));
    std::move(callback).Run(success);
    return;
  }
  std::move(callback).Run(/*success=*/false);
}

void ContextHubPageHandler::GetAllMemoryBankTags(
    GetAllMemoryBankTagsCallback callback) {
  auto* service = ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run({});
    return;
  }

  service->GetAllMemoryBankTags(std::move(callback));
}

void ContextHubPageHandler::GetAllMemoryBankCollections(
    GetAllMemoryBankCollectionsCallback callback) {
  auto* service = ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run({});
    return;
  }

  service->GetAllMemoryBankCollections(std::move(callback));
}

void ContextHubPageHandler::UpdateMemoryBankEntryAnnotations(
    int64_t id,
    browser::context_hub::mojom::MemoryBankEntryAnnotationsPtr annotations,
    UpdateMemoryBankEntryAnnotationsCallback callback) {
  auto* service = ContextHubServiceFactory::GetForProfile(profile_);
  if (!service || !annotations) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  std::vector<std::string> tags =
      std::move(annotations->tags).value_or(std::vector<std::string>{});
  service->UpdateMemoryBankEntryAnnotations(
      id, std::move(tags), std::move(annotations->note),
      std::move(annotations->collection), std::move(callback));
}

namespace {

std::vector<context_hub::TabData> GetOpenUngroupedTabs(
    ContextHubPageHandler::TabProvider* tab_provider) {
  std::vector<context_hub::TabData> tabs;
#if !BUILDFLAG(IS_ANDROID)
  if (tab_provider) {
    for (content::WebContents* tab_contents :
         tab_provider->GetUngroupedTabs()) {
      SessionID session_id = sessions::SessionTabHelper::IdForTab(tab_contents);
      if (session_id.is_valid()) {
        tabs.push_back({session_id.id(),
                        base::UTF16ToUTF8(tab_contents->GetTitle()),
                        tab_contents->GetLastCommittedURL()});
      }
    }
  }
#endif
  return tabs;
}

std::vector<browser::context_hub::mojom::TabInfoPtr> ToMojoTabs(
    const std::vector<context_hub::TabData>& tabs) {
  std::vector<browser::context_hub::mojom::TabInfoPtr> mojo_tabs;
  mojo_tabs.reserve(tabs.size());
  for (const auto& tab : tabs) {
    auto mojo_tab = browser::context_hub::mojom::TabInfo::New();
    mojo_tab->id = tab.id;
    mojo_tab->title = tab.title;
    mojo_tab->url = tab.url;
    mojo_tabs.push_back(std::move(mojo_tab));
  }
  return mojo_tabs;
}

std::vector<browser::context_hub::mojom::ChatMessagePtr> ToMojoChatHistory(
    const std::vector<optimization_guide::proto::ChatHistoryTurn>& history) {
  std::vector<browser::context_hub::mojom::ChatMessagePtr> mojo_history;
  mojo_history.reserve(history.size());
  for (const auto& turn : history) {
    auto mojo_msg = browser::context_hub::mojom::ChatMessage::New();
    mojo_msg->role =
        turn.role() == optimization_guide::proto::ChatHistoryTurn::ROLE_USER
            ? browser::context_hub::mojom::ChatRole::kUser
            : browser::context_hub::mojom::ChatRole::kAssistant;
    mojo_msg->content = turn.message_content();
    mojo_history.push_back(std::move(mojo_msg));
  }
  return mojo_history;
}

}  // namespace

void ContextHubPageHandler::GenerateTabBasedTodos(
    GenerateTabBasedTodosCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service || !tab_provider_) {
    std::move(callback).Run(false);
    return;
  }

  std::vector<base::WeakPtr<content::WebContents>> tab_contents;
  for (content::WebContents* wc : tab_provider_->GetTabs()) {
    if (wc) {
      tab_contents.push_back(wc->GetWeakPtr());
    }
  }

  service->GenerateTabBasedTodos(std::move(tab_contents), std::move(callback));
}

void ContextHubPageHandler::GetTabs(GetTabsCallback callback) {
  std::move(callback).Run(
      ToMojoTabs(GetOpenUngroupedTabs(tab_provider_.get())));
}

void ContextHubPageHandler::RetrieveAndGroupTabs(
    const std::string& user_command,
    RetrieveAndGroupTabsCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service || !tab_provider_) {
    std::move(callback).Run({}, {}, /*llm_response=*/nullptr);
    return;
  }

  service->GroupTabs(
      GetOpenUngroupedTabs(tab_provider_.get()), user_command,
      base::BindOnce(
          [](RetrieveAndGroupTabsCallback callback,
             std::vector<context_hub::TabGroupEntry> groups,
             std::vector<context_hub::TabData> ungrouped_tabs,
             std::string text_response) {
            std::vector<browser::context_hub::mojom::TabGroupPtr> mojo_groups;
            for (const auto& group : groups) {
              auto mojo_group = browser::context_hub::mojom::TabGroup::New();
              mojo_group->label = group.label;
              mojo_group->tabs = ToMojoTabs(group.tabs);
              mojo_groups.push_back(std::move(mojo_group));
            }

            browser::context_hub::mojom::ChatMessagePtr mojo_llm_response;
            if (!text_response.empty()) {
              mojo_llm_response =
                  browser::context_hub::mojom::ChatMessage::New();
              mojo_llm_response->role =
                  browser::context_hub::mojom::ChatRole::kAssistant;
              mojo_llm_response->content = std::move(text_response);
            }

            std::move(callback).Run(std::move(mojo_groups),
                                    ToMojoTabs(ungrouped_tabs),
                                    std::move(mojo_llm_response));
          },
          std::move(callback)));
}

void ContextHubPageHandler::GetExistingTabGroupsAndChats(
    GetExistingTabGroupsAndChatsCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service || !tab_provider_) {
    std::move(callback).Run({}, {}, {});
    return;
  }

  std::vector<context_hub::TabData> open_tabs =
      GetOpenUngroupedTabs(tab_provider_.get());
  std::vector<browser::context_hub::mojom::ChatMessagePtr> mojo_history =
      ToMojoChatHistory(service->GetTabGroupChatHistory());

  if (open_tabs.empty()) {
    std::move(callback).Run({}, {}, std::move(mojo_history));
    return;
  }

  service->GetTabGroups(base::BindOnce(
      [](std::vector<context_hub::TabData> open_tabs,
         std::vector<browser::context_hub::mojom::ChatMessagePtr> mojo_history,
         GetExistingTabGroupsAndChatsCallback callback,
         std::vector<context_hub::TabGroupEntry> stored_groups) {
        base::flat_map<int32_t, size_t> tab_index_map;
        for (size_t i = 0; i < open_tabs.size(); ++i) {
          // Populate the map with tab ID to index.
          tab_index_map.emplace(open_tabs[i].id, i);
        }

        std::vector<browser::context_hub::mojom::TabGroupPtr> mojo_groups;
        // For each stored group, go through each tab in the group and find
        // the corresponding tab by ID in the open tabs list. Delete the tab ID
        // from the map once added to a group.
        for (const auto& entry : stored_groups) {
          std::vector<context_hub::TabData> group_tabs;
          for (int64_t tab_id_64 : entry.tab_ids) {
            int32_t tab_id = static_cast<int32_t>(tab_id_64);
            auto it = tab_index_map.find(tab_id);
            if (it != tab_index_map.end()) {
              group_tabs.push_back(open_tabs[it->second]);
              tab_index_map.erase(it);
            }
          }
          auto mojo_group = browser::context_hub::mojom::TabGroup::New();
          mojo_group->label = entry.label;
          mojo_group->tabs = ToMojoTabs(group_tabs);
          mojo_groups.push_back(std::move(mojo_group));
        }

        // Any remaining tab IDs in the map are ungrouped tabs.
        std::vector<context_hub::TabData> ungrouped_tabs;
        for (const auto& tab : open_tabs) {
          if (tab_index_map.contains(tab.id)) {
            ungrouped_tabs.push_back(tab);
          }
        }

        std::move(callback).Run(std::move(mojo_groups),
                                ToMojoTabs(ungrouped_tabs),
                                std::move(mojo_history));
      },
      std::move(open_tabs), std::move(mojo_history), std::move(callback)));
}

void ContextHubPageHandler::SwitchToTab(int64_t tab_id) {
  if (tab_provider_) {
    tab_provider_->SwitchToTab(tab_id);
  }
}

void ContextHubPageHandler::CloseTab(int64_t tab_id) {
  if (tab_provider_) {
    tab_provider_->CloseTab(tab_id);
    context_hub::ContextHubService* service =
        ContextHubServiceFactory::GetForProfile(profile_);
    if (service) {
      service->DeleteAutoTodoByTabId(tab_id, base::DoNothing());
    }
  }
}

void ContextHubPageHandler::ClearTabGroups(ClearTabGroupsCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run();
    return;
  }

  service->DeleteAllTabGroups(std::move(callback));
}

void ContextHubPageHandler::ClearTabGroupChatHistory(
    ClearTabGroupChatHistoryCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    service->ClearTabGroupChatHistory();
  }
  std::move(callback).Run();
}

void ContextHubPageHandler::AskGeminiWithContext(
    const std::string& user_command,
    const std::vector<int64_t>& memory_bank_entry_ids,
    AskGeminiWithContextCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    auto response = browser::context_hub::mojom::ChatMessage::New();
    response->role = browser::context_hub::mojom::ChatRole::kAssistant;
    response->content = "Service unavailable.";
    std::move(callback).Run(std::move(response));
    return;
  }

  service->ExecuteMemoryBankChat(
      memory_bank_entry_ids, user_command,
      base::BindOnce(
          [](AskGeminiWithContextCallback callback,
             std::optional<std::string> response_text) {
            auto response = browser::context_hub::mojom::ChatMessage::New();
            response->role = browser::context_hub::mojom::ChatRole::kAssistant;
            response->content =
                response_text.value_or("Failed to generate response.");
            std::move(callback).Run(std::move(response));
          },
          std::move(callback)));
}

void ContextHubPageHandler::GetMemoryBankChatHistory(
    GetMemoryBankChatHistoryCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run({});
    return;
  }

  std::move(callback).Run(
      ToMojoChatHistory(service->GetMemoryBankChatHistory()));
}

void ContextHubPageHandler::ClearMemoryBankChatHistory(
    ClearMemoryBankChatHistoryCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    service->ClearMemoryBankChatHistory();
  }
  std::move(callback).Run();
}

void ContextHubPageHandler::ConfirmAllTabGroups(
    ConfirmAllTabGroupsCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service || !tab_provider_) {
    std::move(callback).Run(false);
    return;
  }

  service->GetTabGroups(base::BindOnce(
      [](base::WeakPtr<ContextHubPageHandler> handler,
         base::WeakPtr<context_hub::ContextHubService> service,
         ConfirmAllTabGroupsCallback callback,
         std::vector<context_hub::TabGroupEntry> groups) {
        if (!handler || !service) {
          std::move(callback).Run(false);
          return;
        }
        bool success = handler->tab_provider_->ConfirmTabGroups(groups);
        service->DeleteAllTabGroups(
            base::BindOnce(std::move(callback), success));
      },
      weak_factory_.GetWeakPtr(), service->GetWeakPtr(), std::move(callback)));
}

void ContextHubPageHandler::GetConfirmedTabGroups(
    GetConfirmedTabGroupsCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run({});
    return;
  }

  std::vector<context_hub::TabGroupEntry> entries =
      service->GetConfirmedTabGroups();
  std::vector<browser::context_hub::mojom::TabGroupPtr> groups;
  groups.reserve(entries.size());
  for (const auto& entry : entries) {
    auto mojo_group = browser::context_hub::mojom::TabGroup::New();
    base::Uuid parsed_guid = base::Uuid::ParseCaseInsensitive(entry.id);
    if (parsed_guid.is_valid()) {
      mojo_group->saved_guid = parsed_guid;
    }
    mojo_group->label = entry.label;
    mojo_group->tabs = ToMojoTabs(entry.tabs);
    groups.push_back(std::move(mojo_group));
  }
  std::move(callback).Run(std::move(groups));
}

void ContextHubPageHandler::RemoveConfirmedTabGroup(
    const base::Uuid& saved_guid,
    RemoveConfirmedTabGroupCallback callback) {
  if (!saved_guid.is_valid()) {
    std::move(callback).Run();
    return;
  }

  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service || !tab_provider_) {
    std::move(callback).Run();
    return;
  }

  tab_provider_->UngroupGroupFromTabstripIfOpen(saved_guid);
  service->RemoveConfirmedTabGroup(saved_guid);
  std::move(callback).Run();
}

void ContextHubPageHandler::CloseConfirmedTabGroup(
    const base::Uuid& saved_guid,
    CloseConfirmedTabGroupCallback callback) {
  if (!saved_guid.is_valid()) {
    std::move(callback).Run();
    return;
  }

  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service || !tab_provider_) {
    std::move(callback).Run();
    return;
  }

  tab_provider_->RemoveGroupFromTabstripIfOpen(saved_guid);
  std::move(callback).Run();
}

void ContextHubPageHandler::RemoveAllConfirmedTabGroups(
    RemoveAllConfirmedTabGroupsCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service || !tab_provider_) {
    std::move(callback).Run();
    return;
  }

  for (const context_hub::TabGroupEntry& entry :
       service->GetConfirmedTabGroups()) {
    base::Uuid guid = base::Uuid::ParseCaseInsensitive(entry.id);
    if (guid.is_valid()) {
      tab_provider_->UngroupGroupFromTabstripIfOpen(guid);
    }
  }

  service->RemoveAllConfirmedTabGroups();
  std::move(callback).Run();
}
