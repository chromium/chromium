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
                   int32_t tab_id) override {
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

std::vector<browser::context_hub::mojom::TabInfoPtr>
ContextHubPageHandler::GetTabsInternal() {
  std::vector<browser::context_hub::mojom::TabInfoPtr> tabs;
#if !BUILDFLAG(IS_ANDROID)
  if (!tab_provider_) {
    return tabs;
  }
  for (content::WebContents* tab_contents :
       tab_provider_->GetTabs(web_contents_)) {
    SessionID session_id =
        sessions::SessionTabHelper::IdForTab(tab_contents);
    if (!session_id.is_valid()) {
      continue;
    }
    auto tab_info = browser::context_hub::mojom::TabInfo::New();
    tab_info->id = session_id.id();
    tab_info->title = base::UTF16ToUTF8(tab_contents->GetTitle());
    tab_info->url = tab_contents->GetLastCommittedURL();
    tabs.push_back(std::move(tab_info));
  }
#endif
  return tabs;
}

void ContextHubPageHandler::GetTabs(GetTabsCallback callback) {
  std::move(callback).Run(GetTabsInternal());
}

void ContextHubPageHandler::SwitchToTab(int32_t tab_id) {
  if (tab_provider_) {
    tab_provider_->SwitchToTab(web_contents_, tab_id);
  }
}

void ContextHubPageHandler::ClusterTabs(ClusterTabsCallback callback) {
  // TODO(crbug.com/526733497): Replace this with the real model-based
  // clustering logic once the MES setup is ready.
  std::vector<browser::context_hub::mojom::TabInfoPtr> tabs = GetTabsInternal();
  if (tabs.size() < 2) {
    std::move(callback).Run({}, {});
    return;
  }

  std::vector<browser::context_hub::mojom::TabClusterPtr> clusters;
  std::vector<int32_t> ungrouped_tab_ids;

  static constexpr std::array<const char*, 5> kLabels = {
      "Work", "Shopping", "Research", "Social", "News"};

  size_t current_tab_index = 0;
  size_t cluster_group_number = 1;

  // Cluster tabs sequentially into randomized groups of 2 or 3 tabs.
  while (current_tab_index < tabs.size()) {
    size_t remaining_tabs_count = tabs.size() - current_tab_index;
    // If there is only 1 tab remaining, it cannot form a group. Move it to
    // ungrouped.
    if (remaining_tabs_count == 1) {
      ungrouped_tab_ids.push_back(tabs[current_tab_index]->id);
      break;
    }

    // Randomly select a group size of 2 or 3, bounded by remaining tabs.
    size_t group_size =
        std::min(remaining_tabs_count,
                 static_cast<size_t>(2 + base::RandIntInclusive(0, 1)));

    auto cluster = browser::context_hub::mojom::TabCluster::New();
    // Generate a random label using base::StrCat for cleaner concatenation.
    const char* label_prefix =
        kLabels[base::RandIntInclusive(0, kLabels.size() - 1)];
    cluster->label = base::StrCat(
        {label_prefix, " ", base::NumberToString(cluster_group_number++)});

    for (size_t offset = 0; offset < group_size; ++offset) {
      cluster->tab_ids.push_back(tabs[current_tab_index + offset]->id);
    }
    clusters.push_back(std::move(cluster));
    current_tab_index += group_size;
  }

  std::move(callback).Run(std::move(clusters), std::move(ungrouped_tab_ids));
}
