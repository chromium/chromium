// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/context_hub/context_hub_page_handler.h"

#include <array>
#include <vector>

#include "base/functional/bind.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/context_hub/context_hub_service.h"
#include "chrome/browser/context_hub/context_hub_service_factory.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"
#include "chrome/browser/profiles/profile.h"
#include "components/personal_context/proto/features/auto_todos.pb.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/content/session_tab_helper.h"  // nogncheck
#endif

#if !BUILDFLAG(IS_ANDROID)
class BrowserTabProvider : public ContextHubPageHandler::TabProvider {
 public:
  std::vector<content::WebContents*> GetTabs(
      content::WebContents* web_contents) override {
    std::vector<content::WebContents*> tabs;
    if (!web_contents) {
      return tabs;
    }
    BrowserWindowInterface* browser_window =
        GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
            web_contents);
    if (!browser_window) {
      return tabs;
    }
    TabStripModel* tab_strip_model = browser_window->GetTabStripModel();
    if (!tab_strip_model) {
      return tabs;
    }
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      content::WebContents* tab_contents =
          tab_strip_model->GetWebContentsAt(i);
      if (tab_contents) {
        tabs.push_back(tab_contents);
      }
    }
    return tabs;
  }

  void SwitchToTab(content::WebContents* web_contents,
                   int64_t tab_id) override {
    if (!web_contents) {
      return;
    }
    BrowserWindowInterface* browser_window =
        GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(
            web_contents);
    if (!browser_window) {
      return;
    }
    TabStripModel* tab_strip_model = browser_window->GetTabStripModel();
    if (!tab_strip_model) {
      return;
    }
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      content::WebContents* tab_contents =
          tab_strip_model->GetWebContentsAt(i);
      if (!tab_contents) {
        continue;
      }
      SessionID session_id =
          sessions::SessionTabHelper::IdForTab(tab_contents);
      if (session_id.is_valid() && session_id.id() == tab_id) {
        tab_strip_model->ActivateTabAt(i);
        break;
      }
    }
  }
};
#endif

ContextHubPageHandler::ContextHubPageHandler(
    mojo::PendingReceiver<browser::context_hub::mojom::PageHandler> receiver,
    Profile* profile,
    content::WebContents* web_contents,
    std::unique_ptr<TabProvider> tab_provider)
    : receiver_(this, std::move(receiver)),
      tab_provider_(std::move(tab_provider)),
      profile_(profile),
      web_contents_(web_contents) {
  if (!tab_provider_) {
#if !BUILDFLAG(IS_ANDROID)
    tab_provider_ = std::make_unique<BrowserTabProvider>();
#endif
  }
}

ContextHubPageHandler::~ContextHubPageHandler() = default;

void ContextHubPageHandler::GenerateAutoTodos(
    GenerateAutoTodosCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  service->GenerateAutoTodos(
      base::BindOnce(&ContextHubPageHandler::OnAutoTodosGenerated,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ContextHubPageHandler::OnAutoTodosGenerated(
    GenerateAutoTodosCallback callback,
    std::optional<personal_context::proto::AutoTodosResponse> result) {
  std::optional<std::vector<browser::context_hub::mojom::AutoTodoItemPtr>>
      mojo_todos;
  if (result.has_value() && result.value().todos_size() > 0) {
    mojo_todos.emplace();
    for (const personal_context::proto::AutoTodoItem& todo :
         result.value().todos()) {
      browser::context_hub::mojom::AutoTodoItemPtr mojo_todo =
          browser::context_hub::mojom::AutoTodoItem::New();
      mojo_todo->title = todo.title();
      mojo_todo->description = todo.description();
      mojo_todo->actionable_url = GURL(todo.actionable_url());
      mojo_todo->score =
          std::round(todo.importance_score() * 100.0f) / 100.0f;
      for (const personal_context::proto::SourceReference& ref :
           todo.source_references()) {
        if (ref.has_gmail()) {
          browser::context_hub::mojom::GmailReferencePtr gmail =
              browser::context_hub::mojom::GmailReference::New();
          gmail->message_url = GURL(ref.gmail().message_url());
          mojo_todo->source_references.push_back(
              browser::context_hub::mojom::SourceReference::NewGmail(
                  std::move(gmail)));
        } else if (ref.has_photos()) {
          browser::context_hub::mojom::PhotosReferencePtr photos =
              browser::context_hub::mojom::PhotosReference::New();
          photos->photos_url = GURL(ref.photos().photos_url());
          mojo_todo->source_references.push_back(
              browser::context_hub::mojom::SourceReference::NewPhotos(
                  std::move(photos)));
        }
      }
      mojo_todos->push_back(std::move(mojo_todo));
    }
  }
  std::move(callback).Run(std::move(mojo_todos));
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

  service->DeleteEntries(ids, std::move(callback));
}

namespace {

std::vector<context_hub::TabData> GetOpenTabs(
    ContextHubPageHandler::TabProvider* tab_provider,
    content::WebContents* web_contents) {
  std::vector<context_hub::TabData> tabs;
#if !BUILDFLAG(IS_ANDROID)
  if (tab_provider) {
    for (content::WebContents* tab_contents :
         tab_provider->GetTabs(web_contents)) {
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

void ContextHubPageHandler::GetTabs(GetTabsCallback callback) {
  std::move(callback).Run(
      ToMojoTabs(GetOpenTabs(tab_provider_.get(), web_contents_)));
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
      GetOpenTabs(tab_provider_.get(), web_contents_), user_command,
      base::BindOnce(
          [](RetrieveAndGroupTabsCallback callback,
             std::vector<context_hub::TabGroupEntry> groups,
             std::vector<context_hub::TabData> ungrouped_tabs) {
            std::vector<browser::context_hub::mojom::TabGroupPtr> mojo_groups;
            for (const auto& group : groups) {
              auto mojo_group = browser::context_hub::mojom::TabGroup::New();
              mojo_group->label = group.label;
              mojo_group->tabs = ToMojoTabs(group.tabs);
              mojo_groups.push_back(std::move(mojo_group));
            }

            // TODO(crbug.com/535675010): Add LLM Response.
            std::move(callback).Run(std::move(mojo_groups),
                                    ToMojoTabs(ungrouped_tabs),
                                    /*llm_response=*/nullptr);
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
      GetOpenTabs(tab_provider_.get(), web_contents_);
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
        // the corresponding tab by ID in the open tabs list. Delete the tab ID from
        // the map once added to a group.
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
    tab_provider_->SwitchToTab(web_contents_, tab_id);
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
  auto response = browser::context_hub::mojom::ChatMessage::New();
  response->role = browser::context_hub::mojom::ChatRole::kAssistant;

  if (!service) {
    response->content = "Service unavailable.";
    std::move(callback).Run(std::move(response));
    return;
  }

  // TODO(crbug.com/537894637): Integrate with llm service.
  response->content = base::StringPrintf(
      "Gemini response for prompt: \"%s\"\n\nUsing %zu selected memory ID(s).",
      user_command.c_str(), memory_bank_entry_ids.size());
  std::move(callback).Run(std::move(response));
}
