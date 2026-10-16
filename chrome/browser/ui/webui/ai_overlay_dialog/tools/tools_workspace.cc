// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_collection.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/tools/tools.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/core/page_content_extraction_types.h"
#include "components/page_content_annotations/core/simple_page_content_verbalization.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "ui/base/base_window.h"
#endif

namespace {

#if !BUILDFLAG(IS_ANDROID)
std::string_view StripMarkdownJsonFences(std::string_view input) {
  std::string_view trimmed = base::TrimWhitespaceASCII(input, base::TRIM_ALL);
  if (base::StartsWith(trimmed, "```")) {
    size_t first_newline = trimmed.find('\n');
    if (first_newline != std::string_view::npos) {
      trimmed = trimmed.substr(first_newline + 1);
    }
    if (base::EndsWith(trimmed, "```")) {
      trimmed = trimmed.substr(0, trimmed.size() - 3);
    }
    trimmed = base::TrimWhitespaceASCII(trimmed, base::TRIM_ALL);
  }
  return trimmed;
}

std::string TabGroupColorToString(tab_groups::TabGroupColorId color) {
  switch (color) {
    case tab_groups::TabGroupColorId::kGrey:
      return "grey";
    case tab_groups::TabGroupColorId::kBlue:
      return "blue";
    case tab_groups::TabGroupColorId::kRed:
      return "red";
    case tab_groups::TabGroupColorId::kYellow:
      return "yellow";
    case tab_groups::TabGroupColorId::kGreen:
      return "green";
    case tab_groups::TabGroupColorId::kPink:
      return "pink";
    case tab_groups::TabGroupColorId::kPurple:
      return "purple";
    case tab_groups::TabGroupColorId::kCyan:
      return "cyan";
    case tab_groups::TabGroupColorId::kOrange:
      return "orange";
    default:
      NOTREACHED();
  }
}

tab_groups::TabGroupColorId StringToTabGroupColor(std::string_view color_str) {
  std::string lower = base::ToLowerASCII(color_str);
  if (lower == "grey" || lower == "gray") {
    return tab_groups::TabGroupColorId::kGrey;
  }
  if (lower == "red") {
    return tab_groups::TabGroupColorId::kRed;
  }
  if (lower == "yellow") {
    return tab_groups::TabGroupColorId::kYellow;
  }
  if (lower == "green") {
    return tab_groups::TabGroupColorId::kGreen;
  }
  if (lower == "pink") {
    return tab_groups::TabGroupColorId::kPink;
  }
  if (lower == "purple") {
    return tab_groups::TabGroupColorId::kPurple;
  }
  if (lower == "cyan") {
    return tab_groups::TabGroupColorId::kCyan;
  }
  if (lower == "orange") {
    return tab_groups::TabGroupColorId::kOrange;
  }
  return tab_groups::TabGroupColorId::kBlue;
}

template <typename Fn>
void ForEachBrowserWindow(BrowserWindowInterface* primary_browser, Fn&& fn) {
  if (!primary_browser) {
    return;
  }
  std::set<BrowserWindowInterface*> visited;
  if (Profile* profile = primary_browser->GetProfile()) {
    if (ProfileBrowserCollection* collection =
            ProfileBrowserCollection::GetForProfile(profile)) {
      collection->ForEach(
          [&](BrowserWindowInterface* bwi) {
            visited.insert(bwi);
            fn(bwi);
            return true;
          },
          BrowserCollection::Order::kActivation);
    }
  }
  if (!visited.contains(primary_browser)) {
    fn(primary_browser);
  }
}

bool WebContentsHasUnsavedState(content::WebContents* contents) {
  if (!contents) {
    return false;
  }
  bool has_unsaved = false;
  contents->ForEachRenderFrameHostWithAction(
      [&has_unsaved](content::RenderFrameHost* rfh) {
        if (rfh->IsRenderFrameLive() && rfh->CouldDisplayBeforeUnloadDialog()) {
          has_unsaved = true;
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
  return has_unsaved;
}

struct ResolvedWorkspaceTab {
  raw_ptr<BrowserWindowInterface> browser = nullptr;
  raw_ptr<TabStripModel> tab_strip = nullptr;
  int index = -1;
  raw_ptr<tabs::TabInterface> tab = nullptr;
  raw_ptr<content::WebContents> contents = nullptr;
  int tab_id = -1;
};

std::optional<ResolvedWorkspaceTab> ResolveWorkspaceTabById(
    BrowserWindowInterface* primary_browser,
    int target_tab_id) {
  std::optional<ResolvedWorkspaceTab> matched;
  ForEachBrowserWindow(primary_browser, [&](BrowserWindowInterface* bwi) {
    if (matched.has_value()) {
      return;
    }
    TabStripModel* tab_strip = bwi->GetTabStripModel();
    if (!tab_strip) {
      return;
    }
    for (int i = 0; i < tab_strip->count(); ++i) {
      tabs::TabInterface* tab = tab_strip->GetTabAtIndex(i);
      content::WebContents* contents = tab ? tab->GetContents() : nullptr;
      if (!contents) {
        continue;
      }
      int session_id = sessions::SessionTabHelper::IdForTab(contents).id();
      if (session_id == target_tab_id) {
        matched =
            ResolvedWorkspaceTab{bwi, tab_strip, i, tab, contents, session_id};
        return;
      }
    }
  });
  if (matched.has_value()) {
    return matched;
  }

  // Fallback to 1-based sequential index across windows if no SessionID
  // matched.
  int seq_id = 1;
  ForEachBrowserWindow(primary_browser, [&](BrowserWindowInterface* bwi) {
    if (matched.has_value()) {
      return;
    }
    TabStripModel* tab_strip = bwi->GetTabStripModel();
    if (!tab_strip) {
      return;
    }
    for (int i = 0; i < tab_strip->count(); ++i) {
      tabs::TabInterface* tab = tab_strip->GetTabAtIndex(i);
      content::WebContents* contents = tab ? tab->GetContents() : nullptr;
      if (!contents) {
        continue;
      }
      if (seq_id == target_tab_id) {
        matched =
            ResolvedWorkspaceTab{bwi, tab_strip, i, tab, contents, seq_id};
        return;
      }
      ++seq_id;
    }
  });
  return matched;
}

std::vector<SessionID> CaptureWorkspaceWindowSessionIds(
    BrowserWindowInterface* primary_browser) {
  std::vector<SessionID> window_session_ids;
  ForEachBrowserWindow(primary_browser, [&](BrowserWindowInterface* bwi) {
    if (bwi->GetTabStripModel()) {
      window_session_ids.push_back(bwi->GetSessionID());
    }
  });
  return window_session_ids;
}

BrowserWindowInterface* ResolveBrowserWindowBySessionId(
    BrowserWindowInterface* primary_browser,
    SessionID target_session_id) {
  BrowserWindowInterface* matched = nullptr;
  ForEachBrowserWindow(primary_browser, [&](BrowserWindowInterface* bwi) {
    if (!matched && bwi->GetTabStripModel() &&
        bwi->GetSessionID() == target_session_id) {
      matched = bwi;
    }
  });
  return matched;
}

BrowserWindowInterface* ResolveBrowserWindowByIndex(
    BrowserWindowInterface* primary_browser,
    const std::vector<SessionID>& window_session_ids,
    int target_win_idx) {
  if (!primary_browser || target_win_idx <= 0 ||
      static_cast<size_t>(target_win_idx) > window_session_ids.size()) {
    return nullptr;
  }
  return ResolveBrowserWindowBySessionId(
      primary_browser, window_session_ids[target_win_idx - 1]);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

namespace ttc {

struct AiOverlayTools::WorkspaceUndoSnapshot {
#if !BUILDFLAG(IS_ANDROID)
  std::map<int, std::optional<tab_groups::TabGroupId>> tab_groups_by_tab_id;
  std::map<tab_groups::TabGroupId, tab_groups::TabGroupVisualData>
      group_visuals;
  std::map<SessionID, std::vector<int>> tab_order_by_window;
  std::map<int, GURL> tab_urls_by_tab_id;
  std::map<int, std::u16string> tab_titles_by_tab_id;
  std::map<int, bool> tab_pinned_by_tab_id;
  std::map<int, bool> tab_muted_by_tab_id;
  int closed_tabs_count = 0;
#endif
};

void AiOverlayTools::WorkspaceUndoSnapshotDeleter::operator()(
    WorkspaceUndoSnapshot* ptr) const {
  delete ptr;
}

base::DictValue AiOverlayTools::BuildWorkspaceSnapshot() {
#if !BUILDFLAG(IS_ANDROID)
  if (!browser_) {
    return base::DictValue();
  }
  Profile* profile = browser_->GetProfile();
  if (!profile) {
    return base::DictValue();
  }

  page_content_annotations::PageContentExtractionService* pces =
      page_content_annotations::PageContentExtractionServiceFactory::
          GetForProfile(profile);

  tab_groups::TabGroupSyncService* sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile);

  base::DictValue root;
  base::ListValue windows_list;

  int next_tab_seq_id = 1;
  int next_group_seq_id = 1;

  std::map<tab_groups::TabGroupId, int> local_group_id_to_seq;
  std::set<tab_groups::TabGroupId> observed_open_group_ids;

  int window_index = 0;
  ProfileBrowserCollection* browser_collection =
      ProfileBrowserCollection::GetForProfile(profile);
  if (browser_collection) {
    browser_collection->ForEach(
        [&](BrowserWindowInterface* bwi) {
          TabStripModel* tab_strip = bwi->GetTabStripModel();
          if (!tab_strip) {
            return true;
          }

          base::DictValue win_dict;
          win_dict.Set("window_index", ++window_index);
          win_dict.Set("is_active", bwi == browser_);
          win_dict.Set("tab_count", tab_strip->count());

          // 1. Tab groups in this window
          base::ListValue groups_in_window;
          if (tab_strip->group_model()) {
            for (const tab_groups::TabGroupId& group_id :
                 tab_strip->group_model()->ListTabGroups()) {
              TabGroup* group = tab_strip->group_model()->GetTabGroup(group_id);
              if (!group) {
                continue;
              }
              int group_seq_id = next_group_seq_id++;
              local_group_id_to_seq.insert_or_assign(group_id, group_seq_id);
              observed_open_group_ids.insert(group_id);

              base::DictValue group_dict;
              group_dict.Set("group_id", group_seq_id);
              const tab_groups::TabGroupVisualData* visual =
                  group->visual_data();
              if (visual) {
                group_dict.Set("title", base::UTF16ToUTF8(visual->title()));
                group_dict.Set("color", TabGroupColorToString(visual->color()));
                group_dict.Set("is_collapsed", visual->is_collapsed());
              } else {
                group_dict.Set("title", "");
                group_dict.Set("color", "default");
                group_dict.Set("is_collapsed", false);
              }

              // Check if saved/shared in sync service
              if (sync_service) {
                std::optional<tab_groups::SavedTabGroup> saved_group =
                    sync_service->GetGroup(group_id);
                if (saved_group) {
                  group_dict.Set("is_saved", true);
                  group_dict.Set("is_shared",
                                 saved_group->collaboration_id().has_value());
                } else {
                  group_dict.Set("is_saved", false);
                  group_dict.Set("is_shared", false);
                }
              }

              groups_in_window.Append(std::move(group_dict));
            }
          }
          win_dict.Set("tab_groups", std::move(groups_in_window));

          // 2. Tabs in this window
          base::ListValue tabs_list;
          for (int i = 0; i < tab_strip->count(); ++i) {
            tabs::TabInterface* tab = tab_strip->GetTabAtIndex(i);
            if (!tab) {
              continue;
            }
            content::WebContents* contents = tab->GetContents();
            if (!contents) {
              continue;
            }

            int session_tab_id =
                sessions::SessionTabHelper::IdForTab(contents).id();
            int tab_seq_id =
                session_tab_id > 0 ? session_tab_id : next_tab_seq_id;
            ++next_tab_seq_id;

            base::DictValue tab_dict;
            tab_dict.Set("tab_id", tab_seq_id);
            tab_dict.Set("title", base::UTF16ToUTF8(contents->GetTitle()));
            tab_dict.Set("url", contents->GetVisibleURL().spec());
            tab_dict.Set("is_active", tab_strip->active_index() == i);
            tab_dict.Set("is_pinned", tab->IsPinned());

            // Group association
            std::optional<tab_groups::TabGroupId> group_id = tab->GetGroup();
            if (group_id.has_value() &&
                local_group_id_to_seq.contains(*group_id)) {
              int g_seq = local_group_id_to_seq[*group_id];
              tab_dict.Set("group_id", g_seq);
              if (tab_strip->group_model()) {
                TabGroup* group =
                    tab_strip->group_model()->GetTabGroup(*group_id);
                if (group && group->visual_data()) {
                  tab_dict.Set(
                      "group_title",
                      base::UTF16ToUTF8(group->visual_data()->title()));
                }
              }
            }

            // Memory usage
            int memory_mb = 0;
            bool is_high_memory = false;
            TabResourceUsageTabHelper* resource_helper =
                TabResourceUsageTabHelper::From(tab);
            if (resource_helper) {
              memory_mb = base::saturated_cast<int>(
                  resource_helper->GetMemoryUsage().InMiB());
              auto usage = resource_helper->resource_usage();
              if (usage) {
                is_high_memory = usage->is_high_memory_usage();
              }
            }
            tab_dict.Set("memory_mb", memory_mb);
            tab_dict.Set("is_high_memory", is_high_memory);

            // Idle time
            int idle_minutes = 0;
            base::TimeTicks last_active = contents->GetLastActiveTimeTicks();
            if (!last_active.is_null()) {
              idle_minutes = std::max(
                  0, base::saturated_cast<int>(
                         (base::TimeTicks::Now() - last_active).InMinutes()));
            }
            tab_dict.Set("last_active_minutes_ago", idle_minutes);

            // State indicators
            tab_dict.Set("has_unsaved_state",
                         WebContentsHasUnsavedState(contents));
            tab_dict.Set("is_audible", contents->IsCurrentlyAudible());
            tab_dict.Set("is_muted", contents->IsAudioMuted());
            tab_dict.Set("is_discarded", contents->WasDiscarded());

            // ~220 char content snippet
            std::string content_snippet;
            if (pces) {
              std::optional<
                  page_content_annotations::ExtractedPageContentResult>
                  extraction =
                      pces->GetExtractedPageContentAndEligibilityForPage(
                          contents->GetPrimaryPage());
              if (extraction && extraction->is_eligible_for_server_upload &&
                  extraction->page_content) {
                std::vector<std::string> passages = page_content_annotations::
                    CreatePassagesFromAnnotatedPageContent(
                        extraction->page_content->data,
                        /*max_passages_per_page=*/1);
                if (!passages.empty() && !passages[0].empty()) {
                  content_snippet = passages[0];
                  base::ReplaceChars(content_snippet, "\r\n\t", " ",
                                     &content_snippet);
                  if (content_snippet.length() > 220) {
                    base::TruncateUTF8ToByteSize(content_snippet, 217,
                                                 &content_snippet);
                    content_snippet += "...";
                  }
                }
              }
            }
            tab_dict.Set("content_snippet", content_snippet);

            tabs_list.Append(std::move(tab_dict));
          }
          win_dict.Set("tabs", std::move(tabs_list));

          windows_list.Append(std::move(win_dict));
          return true;
        },
        BrowserCollection::Order::kActivation);
  }
  root.Set("windows", std::move(windows_list));

  // 3. Saved / closed tab groups
  base::ListValue saved_groups_list;
  if (sync_service) {
    for (const tab_groups::SavedTabGroup& saved_group :
         sync_service->GetAllGroups()) {
      // Check if already open locally in one of the windows
      bool is_open =
          saved_group.local_group_id().has_value() &&
          observed_open_group_ids.contains(*saved_group.local_group_id());
      if (is_open) {
        continue;
      }

      base::DictValue saved_dict;
      saved_dict.Set("saved_guid",
                     saved_group.saved_guid().AsLowercaseString());
      saved_dict.Set("title", base::UTF16ToUTF8(saved_group.title()));
      saved_dict.Set("color", TabGroupColorToString(saved_group.color()));
      saved_dict.Set("is_shared", saved_group.collaboration_id().has_value());
      saved_dict.Set("is_open", false);

      base::ListValue saved_tabs_list;
      for (const tab_groups::SavedTabGroupTab& tab : saved_group.saved_tabs()) {
        base::DictValue stab_dict;
        stab_dict.Set("title", base::UTF16ToUTF8(tab.title()));
        stab_dict.Set("url", tab.url().spec());
        saved_tabs_list.Append(std::move(stab_dict));
      }
      saved_dict.Set("tabs", std::move(saved_tabs_list));

      saved_groups_list.Append(std::move(saved_dict));
    }
  }
  root.Set("saved_tab_groups", std::move(saved_groups_list));

  return root;
#else
  return base::DictValue();
#endif  // !BUILDFLAG(IS_ANDROID)
}

void AiOverlayTools::SaveWorkspaceUndoSnapshot() {
#if !BUILDFLAG(IS_ANDROID)
  workspace_undo_snapshot_.reset(new WorkspaceUndoSnapshot());
  ForEachBrowserWindow(browser_, [&](BrowserWindowInterface* bwi) {
    TabStripModel* tab_strip = bwi->GetTabStripModel();
    if (!tab_strip) {
      return;
    }
    if (tab_strip->group_model()) {
      for (const tab_groups::TabGroupId& gid :
           tab_strip->group_model()->ListTabGroups()) {
        TabGroup* group = tab_strip->group_model()->GetTabGroup(gid);
        if (group && group->visual_data()) {
          workspace_undo_snapshot_->group_visuals.insert_or_assign(
              gid, *group->visual_data());
        }
      }
    }
    std::vector<int> window_tab_order;
    for (int i = 0; i < tab_strip->count(); ++i) {
      tabs::TabInterface* tab = tab_strip->GetTabAtIndex(i);
      content::WebContents* contents = tab ? tab->GetContents() : nullptr;
      if (!contents) {
        continue;
      }
      int tab_id = sessions::SessionTabHelper::IdForTab(contents).id();
      if (tab_id > 0) {
        workspace_undo_snapshot_->tab_groups_by_tab_id[tab_id] =
            tab->GetGroup();
        workspace_undo_snapshot_->tab_urls_by_tab_id[tab_id] =
            contents->GetVisibleURL();
        workspace_undo_snapshot_->tab_titles_by_tab_id[tab_id] =
            contents->GetTitle();
        workspace_undo_snapshot_->tab_pinned_by_tab_id[tab_id] =
            tab->IsPinned();
        workspace_undo_snapshot_->tab_muted_by_tab_id[tab_id] =
            contents->IsAudioMuted();
        window_tab_order.push_back(tab_id);
      }
    }
    workspace_undo_snapshot_->tab_order_by_window[bwi->GetSessionID()] =
        std::move(window_tab_order);
  });
#endif  // !BUILDFLAG(IS_ANDROID)
}

bool AiOverlayTools::RestoreWorkspaceUndoSnapshot() {
#if !BUILDFLAG(IS_ANDROID)
  if (!workspace_undo_snapshot_ || !browser_) {
    return false;
  }
  std::unique_ptr<WorkspaceUndoSnapshot, WorkspaceUndoSnapshotDeleter>
      snapshot = std::move(workspace_undo_snapshot_);

  // 1. Reopen tabs closed in the last action batch.
  for (int i = 0; i < snapshot->closed_tabs_count; ++i) {
    chrome::RestoreTab(browser_);
  }

  // 1b. TabRestoreService mints a new SessionTabHelper ID for each restored
  // WebContents. Map any missing prior_tid to newly restored tabs by matching
  // (URL, title) so window placement, group membership, and tab order are
  // restored accurately.
  if (snapshot->closed_tabs_count > 0) {
    std::set<int> current_open_tids;
    struct RestoredCandidate {
      int current_tid;
      GURL url;
      std::u16string title;
      bool matched = false;
    };
    std::vector<RestoredCandidate> new_tab_candidates;
    ForEachBrowserWindow(browser_, [&](BrowserWindowInterface* bwi) {
      TabStripModel* ts = bwi->GetTabStripModel();
      if (!ts) {
        return;
      }
      for (int i = 0; i < ts->count(); ++i) {
        content::WebContents* wc = ts->GetWebContentsAt(i);
        if (!wc) {
          continue;
        }
        int tid = sessions::SessionTabHelper::IdForTab(wc).id();
        current_open_tids.insert(tid);
        if (!snapshot->tab_urls_by_tab_id.contains(tid)) {
          new_tab_candidates.push_back(
              {tid, wc->GetVisibleURL(), wc->GetTitle(), false});
        }
      }
    });

    std::map<int, int> remapped_tids;
    for (const auto& [prior_tid, prior_url] : snapshot->tab_urls_by_tab_id) {
      if (current_open_tids.contains(prior_tid)) {
        continue;
      }
      const std::u16string& prior_title =
          snapshot->tab_titles_by_tab_id[prior_tid];
      int matched_new_tid = -1;
      for (auto& cand : new_tab_candidates) {
        if (!cand.matched && cand.url == prior_url &&
            cand.title == prior_title) {
          cand.matched = true;
          matched_new_tid = cand.current_tid;
          break;
        }
      }
      if (matched_new_tid < 0) {
        for (auto& cand : new_tab_candidates) {
          if (!cand.matched && cand.url == prior_url) {
            cand.matched = true;
            matched_new_tid = cand.current_tid;
            break;
          }
        }
      }
      if (matched_new_tid > 0) {
        remapped_tids[prior_tid] = matched_new_tid;
      }
    }

    for (const auto& [old_tid, new_tid] : remapped_tids) {
      if (auto it = snapshot->tab_groups_by_tab_id.find(old_tid);
          it != snapshot->tab_groups_by_tab_id.end()) {
        snapshot->tab_groups_by_tab_id[new_tid] = it->second;
      }
      if (auto it = snapshot->tab_pinned_by_tab_id.find(old_tid);
          it != snapshot->tab_pinned_by_tab_id.end()) {
        snapshot->tab_pinned_by_tab_id[new_tid] = it->second;
      }
      if (auto it = snapshot->tab_muted_by_tab_id.find(old_tid);
          it != snapshot->tab_muted_by_tab_id.end()) {
        snapshot->tab_muted_by_tab_id[new_tid] = it->second;
      }
      for (auto& [win_id, order_vec] : snapshot->tab_order_by_window) {
        std::ranges::replace(order_vec, old_tid, new_tid);
      }
    }
  }

  // 2. Move any tabs that were relocated across windows back to their prior
  // window if that window is still open.
  for (const auto& [target_win_id, prior_tab_ids] :
       snapshot->tab_order_by_window) {
    BrowserWindowInterface* target_bwi =
        ResolveBrowserWindowBySessionId(browser_, target_win_id);
    if (!target_bwi) {
      continue;
    }
    std::map<BrowserWindowInterface*, std::vector<int>>
        cross_win_indices_by_src;
    for (int prior_tid : prior_tab_ids) {
      std::optional<ResolvedWorkspaceTab> resolved =
          ResolveWorkspaceTabById(browser_, prior_tid);
      if (!resolved.has_value() || !resolved->browser ||
          resolved->browser.get() == target_bwi) {
        continue;
      }
      cross_win_indices_by_src[resolved->browser.get()].push_back(
          resolved->index);
    }
    for (auto& [src_bwi, indices] : cross_win_indices_by_src) {
      if (!src_bwi || indices.empty()) {
        continue;
      }
      std::ranges::sort(indices);
      indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
      chrome::MoveTabsToExistingWindow(src_bwi, target_bwi, indices);
    }
  }

  // Collect distinct prior TabGroupIds to restore.
  std::vector<tab_groups::TabGroupId> distinct_prior_groups;
  for (const auto& [tab_id, prior_group] : snapshot->tab_groups_by_tab_id) {
    if (prior_group.has_value() &&
        !std::ranges::contains(distinct_prior_groups, *prior_group)) {
      distinct_prior_groups.push_back(*prior_group);
    }
  }

  // 3. Restore pin/mute state, tab ordering, and tab group memberships/visuals.
  for (const auto& [tab_id, prior_muted] : snapshot->tab_muted_by_tab_id) {
    std::optional<ResolvedWorkspaceTab> resolved =
        ResolveWorkspaceTabById(browser_, tab_id);
    if (resolved.has_value() && resolved->contents &&
        resolved->contents->IsAudioMuted() != prior_muted) {
      resolved->contents->SetAudioMuted(prior_muted);
    }
  }
  for (const auto& [tab_id, prior_pinned] : snapshot->tab_pinned_by_tab_id) {
    std::optional<ResolvedWorkspaceTab> resolved =
        ResolveWorkspaceTabById(browser_, tab_id);
    if (resolved.has_value() && resolved->tab_strip && resolved->tab &&
        resolved->tab->IsPinned() != prior_pinned) {
      resolved->tab_strip->SetTabPinned(resolved->index, prior_pinned);
    }
  }

  ForEachBrowserWindow(browser_, [&](BrowserWindowInterface* bwi) {
    TabStripModel* tab_strip = bwi->GetTabStripModel();
    if (!tab_strip) {
      return;
    }

    // Restore prior tab ordering within this window first so grouped tabs are
    // contiguous at their snapshot positions before group memberships are
    // restored.
    auto order_it = snapshot->tab_order_by_window.find(bwi->GetSessionID());
    if (order_it != snapshot->tab_order_by_window.end()) {
      int target_slot = 0;
      for (int prior_tid : order_it->second) {
        std::optional<ResolvedWorkspaceTab> resolved =
            ResolveWorkspaceTabById(browser_, prior_tid);
        if (!resolved.has_value() || resolved->tab_strip != tab_strip) {
          continue;
        }
        int current_slot = resolved->index;
        if (current_slot != target_slot) {
          tab_strip->MoveWebContentsAt(current_slot, target_slot,
                                       /*select_after_move=*/false);
        }
        ++target_slot;
      }
    }

    // Next, ungroup tabs that were ungrouped before the last mutation.
    std::vector<int> indices_to_ungroup;
    for (int i = 0; i < tab_strip->count(); ++i) {
      tabs::TabInterface* tab = tab_strip->GetTabAtIndex(i);
      content::WebContents* contents = tab ? tab->GetContents() : nullptr;
      if (!contents || !tab->GetGroup().has_value()) {
        continue;
      }
      int tab_id = sessions::SessionTabHelper::IdForTab(contents).id();
      auto it = snapshot->tab_groups_by_tab_id.find(tab_id);
      if (it != snapshot->tab_groups_by_tab_id.end() &&
          !it->second.has_value()) {
        indices_to_ungroup.push_back(i);
      }
    }
    if (!indices_to_ungroup.empty()) {
      tab_strip->RemoveFromGroup(indices_to_ungroup);
    }

    // Finally, restore each prior group and its visual metadata.
    if (tab_strip->SupportsTabGroups()) {
      for (const tab_groups::TabGroupId& target_gid : distinct_prior_groups) {
        std::vector<int> indices_for_group;
        for (int i = 0; i < tab_strip->count(); ++i) {
          tabs::TabInterface* tab = tab_strip->GetTabAtIndex(i);
          content::WebContents* contents = tab ? tab->GetContents() : nullptr;
          if (!contents) {
            continue;
          }
          int tab_id = sessions::SessionTabHelper::IdForTab(contents).id();
          auto it = snapshot->tab_groups_by_tab_id.find(tab_id);
          if (it != snapshot->tab_groups_by_tab_id.end() &&
              it->second == target_gid) {
            indices_for_group.push_back(i);
          }
        }
        if (!indices_for_group.empty()) {
          tab_strip->AddToGroupForRestore(indices_for_group, target_gid);
          auto vis_it = snapshot->group_visuals.find(target_gid);
          if (vis_it != snapshot->group_visuals.end()) {
            tab_strip->ChangeTabGroupVisuals(target_gid, vis_it->second,
                                             /*is_customized=*/true);
          }
        }
      }
    }
  });

  return true;
#else
  return false;
#endif  // !BUILDFLAG(IS_ANDROID)
}

base::DictValue AiOverlayTools::ExecuteWorkspaceSubAgentOutput(
    std::string_view raw_output) {
#if !BUILDFLAG(IS_ANDROID)
  std::string_view cleaned = StripMarkdownJsonFences(raw_output);
  std::optional<base::DictValue> dict =
      base::JSONReader::ReadDict(cleaned, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!dict) {
    size_t first_brace = cleaned.find('{');
    size_t last_brace = cleaned.rfind('}');
    if (first_brace != std::string_view::npos &&
        last_brace != std::string_view::npos && last_brace > first_brace) {
      dict = base::JSONReader::ReadDict(
          cleaned.substr(first_brace, last_brace - first_brace + 1),
          base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    }
  }
  if (!dict) {
    DVLOG(1) << "AiOverlayTools: Failed to parse workspace sub-agent output as "
                "JSON dict:\n"
             << raw_output;
    base::DictValue fallback;
    fallback.Set("facts", std::string(cleaned));
    return fallback;
  }

  std::string facts;
  if (const std::string* f = dict->FindString("facts")) {
    facts = *f;
  } else if (const std::string* r = dict->FindString("response")) {
    facts = *r;
  }

  base::ListValue actions_taken;
  base::ListValue skipped;

  const base::ListValue* actions = dict->FindList("actions");
  if (actions && !actions->empty() && browser_) {
    // Capture the 1-based window index -> SessionID mapping before executing
    // the batch so window indices stay stable even if actions activate, open,
    // or close windows mid-batch.
    const std::vector<SessionID> batch_window_session_ids =
        CaptureWorkspaceWindowSessionIds(browser_);

    // Save a 1-step undo snapshot if the batch contains any mutating action.
    bool has_mutating_action = false;
    for (const base::Value& action_val : *actions) {
      if (!action_val.is_dict()) {
        continue;
      }
      const std::string* type = action_val.GetDict().FindString("type");
      if (!type) {
        continue;
      }
      if (*type == "create_group" || *type == "add_to_group" ||
          *type == "ungroup_tabs" || *type == "close_tabs" ||
          *type == "move_to_new_window" || *type == "move_tabs_to_window" ||
          *type == "merge_windows" || *type == "reorder_tabs" ||
          *type == "pin_tabs" || *type == "unpin_tabs" ||
          *type == "mute_tabs" || *type == "unmute_tabs" ||
          *type == "discard_tabs") {
        has_mutating_action = true;
        break;
      }
    }
    std::unique_ptr<WorkspaceUndoSnapshot, WorkspaceUndoSnapshotDeleter>
        prior_undo_snapshot;
    if (has_mutating_action) {
      prior_undo_snapshot = std::move(workspace_undo_snapshot_);
      SaveWorkspaceUndoSnapshot();
    }

    for (const base::Value& action_val : *actions) {
      if (!action_val.is_dict()) {
        continue;
      }
      const base::DictValue& action = action_val.GetDict();
      const std::string* type = action.FindString("type");
      if (!type) {
        continue;
      }

      if (*type == "create_group") {
        std::string title =
            action.FindString("title") ? *action.FindString("title") : "Group";
        std::string color_str =
            action.FindString("color") ? *action.FindString("color") : "blue";
        const base::ListValue* tab_ids = action.FindList("tab_ids");
        if (!tab_ids || tab_ids->empty()) {
          continue;
        }

        std::map<TabStripModel*, std::vector<int>> indices_by_strip;
        for (const base::Value& tid_val : *tab_ids) {
          std::optional<int> tid = tid_val.GetIfInt();
          if (!tid.has_value()) {
            continue;
          }
          std::optional<ResolvedWorkspaceTab> resolved =
              ResolveWorkspaceTabById(browser_, *tid);
          if (!resolved.has_value()) {
            base::DictValue skip_entry;
            skip_entry.Set("tab_id", *tid);
            skip_entry.Set("reason", "not_found");
            skipped.Append(std::move(skip_entry));
            continue;
          }
          indices_by_strip[resolved->tab_strip].push_back(resolved->index);
        }

        for (auto& [tab_strip, indices] : indices_by_strip) {
          if (!tab_strip || !tab_strip->SupportsTabGroups() ||
              indices.empty()) {
            continue;
          }
          std::ranges::sort(indices);
          indices.erase(std::unique(indices.begin(), indices.end()),
                        indices.end());
          tab_groups::TabGroupId group_id = tab_strip->AddToNewGroup(indices);
          tab_groups::TabGroupVisualData visual_data(
              base::UTF8ToUTF16(title), StringToTabGroupColor(color_str),
              /*is_collapsed=*/false);
          tab_strip->ChangeTabGroupVisuals(group_id, std::move(visual_data),
                                           /*is_customized=*/true);

          base::DictValue taken;
          taken.Set("type", "create_group");
          taken.Set("title", title);
          taken.Set("color", color_str);
          taken.Set("tab_count", static_cast<int>(indices.size()));
          actions_taken.Append(std::move(taken));
        }
      } else if (*type == "add_to_group") {
        std::string group_title =
            action.FindString("group_title")
                ? *action.FindString("group_title")
                : (action.FindString("title") ? *action.FindString("title")
                                              : "");
        std::optional<int> target_group_seq = action.FindInt("group_id");
        const base::ListValue* tab_ids = action.FindList("tab_ids");
        if (!tab_ids || tab_ids->empty()) {
          continue;
        }

        std::map<TabStripModel*, std::vector<int>> indices_by_strip;
        for (const base::Value& tid_val : *tab_ids) {
          std::optional<int> tid = tid_val.GetIfInt();
          if (!tid.has_value()) {
            continue;
          }
          std::optional<ResolvedWorkspaceTab> resolved =
              ResolveWorkspaceTabById(browser_, *tid);
          if (!resolved.has_value()) {
            base::DictValue skip_entry;
            skip_entry.Set("tab_id", *tid);
            skip_entry.Set("reason", "not_found");
            skipped.Append(std::move(skip_entry));
            continue;
          }
          indices_by_strip[resolved->tab_strip].push_back(resolved->index);
        }

        // Build global 1-based group_id -> (TabStripModel*, TabGroupId) map
        // across all windows in the same order as BuildWorkspaceSnapshot().
        std::map<int, std::pair<TabStripModel*, tab_groups::TabGroupId>>
            group_by_global_seq;
        int global_seq = 1;
        ForEachBrowserWindow(browser_, [&](BrowserWindowInterface* bwi) {
          TabStripModel* ts = bwi->GetTabStripModel();
          if (!ts || !ts->group_model()) {
            return;
          }
          for (const tab_groups::TabGroupId& gid :
               ts->group_model()->ListTabGroups()) {
            if (ts->group_model()->GetTabGroup(gid)) {
              group_by_global_seq.emplace(global_seq++,
                                          std::make_pair(ts, gid));
            }
          }
        });

        for (auto& [tab_strip, indices] : indices_by_strip) {
          if (!tab_strip || !tab_strip->SupportsTabGroups() ||
              !tab_strip->group_model() || indices.empty()) {
            continue;
          }
          std::ranges::sort(indices);
          indices.erase(std::unique(indices.begin(), indices.end()),
                        indices.end());

          std::optional<tab_groups::TabGroupId> matched_gid;
          if (target_group_seq.has_value()) {
            auto seq_it = group_by_global_seq.find(*target_group_seq);
            if (seq_it != group_by_global_seq.end() &&
                seq_it->second.first == tab_strip) {
              matched_gid = seq_it->second.second;
              TabGroup* grp =
                  tab_strip->group_model()->GetTabGroup(*matched_gid);
              if (group_title.empty() && grp && grp->visual_data()) {
                group_title = base::UTF16ToUTF8(grp->visual_data()->title());
              }
            }
          }

          if (!matched_gid.has_value() && !group_title.empty()) {
            std::string lower_target = base::ToLowerASCII(group_title);
            for (const tab_groups::TabGroupId& gid :
                 tab_strip->group_model()->ListTabGroups()) {
              TabGroup* grp = tab_strip->group_model()->GetTabGroup(gid);
              if (grp && grp->visual_data() &&
                  base::ToLowerASCII(base::UTF16ToUTF8(
                      grp->visual_data()->title())) == lower_target) {
                matched_gid = gid;
                break;
              }
            }
          }

          if (matched_gid.has_value()) {
            tab_strip->AddToExistingGroup(indices, *matched_gid);
          } else {
            tab_groups::TabGroupId new_gid = tab_strip->AddToNewGroup(indices);
            tab_groups::TabGroupVisualData visual_data(
                base::UTF8ToUTF16(group_title.empty() ? "Group" : group_title),
                tab_groups::TabGroupColorId::kBlue, /*is_collapsed=*/false);
            tab_strip->ChangeTabGroupVisuals(new_gid, std::move(visual_data),
                                             /*is_customized=*/true);
          }

          base::DictValue taken;
          taken.Set("type", "add_to_group");
          taken.Set("group_title", group_title);
          taken.Set("tab_count", static_cast<int>(indices.size()));
          actions_taken.Append(std::move(taken));
        }
      } else if (*type == "ungroup_tabs") {
        const base::ListValue* tab_ids = action.FindList("tab_ids");
        if (!tab_ids || tab_ids->empty()) {
          continue;
        }

        std::map<TabStripModel*, std::vector<int>> indices_by_strip;
        for (const base::Value& tid_val : *tab_ids) {
          std::optional<int> tid = tid_val.GetIfInt();
          if (!tid.has_value()) {
            continue;
          }
          std::optional<ResolvedWorkspaceTab> resolved =
              ResolveWorkspaceTabById(browser_, *tid);
          if (!resolved.has_value()) {
            base::DictValue skip_entry;
            skip_entry.Set("tab_id", *tid);
            skip_entry.Set("reason", "not_found");
            skipped.Append(std::move(skip_entry));
            continue;
          }
          indices_by_strip[resolved->tab_strip].push_back(resolved->index);
        }

        for (auto& [tab_strip, indices] : indices_by_strip) {
          if (!tab_strip || indices.empty()) {
            continue;
          }
          std::ranges::sort(indices);
          indices.erase(std::unique(indices.begin(), indices.end()),
                        indices.end());
          tab_strip->RemoveFromGroup(indices);

          base::DictValue taken;
          taken.Set("type", "ungroup_tabs");
          taken.Set("tab_count", static_cast<int>(indices.size()));
          actions_taken.Append(std::move(taken));
        }
      } else if (*type == "close_tabs") {
        const base::ListValue* tab_ids = action.FindList("tab_ids");
        if (!tab_ids || tab_ids->empty()) {
          continue;
        }

        // Map TabStripModel* -> list of (tab_strip_index, tab_id).
        std::map<TabStripModel*, std::vector<std::pair<int, int>>>
            to_close_by_strip;
        for (const base::Value& tid_val : *tab_ids) {
          std::optional<int> tid = tid_val.GetIfInt();
          if (!tid.has_value()) {
            continue;
          }
          std::optional<ResolvedWorkspaceTab> resolved =
              ResolveWorkspaceTabById(browser_, *tid);
          if (!resolved.has_value()) {
            base::DictValue skip_entry;
            skip_entry.Set("tab_id", *tid);
            skip_entry.Set("reason", "not_found");
            skipped.Append(std::move(skip_entry));
            continue;
          }
          // Safety guardrail: Never close tabs with unsaved form/editor state.
          if (WebContentsHasUnsavedState(resolved->contents)) {
            base::DictValue skip_entry;
            skip_entry.Set("tab_id", *tid);
            skip_entry.Set("title",
                           base::UTF16ToUTF8(resolved->contents->GetTitle()));
            skip_entry.Set("reason", "has_unsaved_state");
            skipped.Append(std::move(skip_entry));
            continue;
          }
          to_close_by_strip[resolved->tab_strip].emplace_back(resolved->index,
                                                              *tid);
        }

        for (auto& [tab_strip, pairs] : to_close_by_strip) {
          if (!tab_strip || pairs.empty()) {
            continue;
          }
          // Sort descending by index so closing higher indices first never
          // invalidates lower indices.
          std::sort(
              pairs.begin(), pairs.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
          pairs.erase(std::unique(pairs.begin(), pairs.end(),
                                  [](const auto& a, const auto& b) {
                                    return a.first == b.first;
                                  }),
                      pairs.end());

          int closed_count = 0;
          for (const auto& [idx, tid] : pairs) {
            // Safety guardrail: Never close the last remaining tab in a window.
            if (tab_strip->count() <= 1) {
              base::DictValue skip_entry;
              skip_entry.Set("tab_id", tid);
              skip_entry.Set("reason", "cannot_close_last_tab_in_window");
              skipped.Append(std::move(skip_entry));
              continue;
            }
            tab_strip->CloseWebContentsAt(
                idx, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
            ++closed_count;
            if (workspace_undo_snapshot_) {
              ++workspace_undo_snapshot_->closed_tabs_count;
            }
          }
          if (closed_count > 0) {
            base::DictValue taken;
            taken.Set("type", "close_tabs");
            taken.Set("tab_count", closed_count);
            actions_taken.Append(std::move(taken));
          }
        }
      } else if (*type == "activate_tab") {
        std::optional<int> tid = action.FindInt("tab_id");
        if (!tid.has_value()) {
          continue;
        }
        std::optional<ResolvedWorkspaceTab> resolved =
            ResolveWorkspaceTabById(browser_, *tid);
        if (!resolved.has_value()) {
          base::DictValue skip_entry;
          skip_entry.Set("tab_id", *tid);
          skip_entry.Set("reason", "not_found");
          skipped.Append(std::move(skip_entry));
          continue;
        }
        if (resolved->tab && resolved->tab->GetGroup().has_value() &&
            resolved->tab_strip->SupportsTabGroups() &&
            resolved->tab_strip->group_model()) {
          tab_groups::TabGroupId gid = *resolved->tab->GetGroup();
          TabGroup* grp = resolved->tab_strip->group_model()->GetTabGroup(gid);
          if (grp && grp->visual_data() && grp->visual_data()->is_collapsed()) {
            tab_groups::TabGroupVisualData expanded(grp->visual_data()->title(),
                                                    grp->visual_data()->color(),
                                                    /*is_collapsed=*/false);
            resolved->tab_strip->ChangeTabGroupVisuals(gid, std::move(expanded),
                                                       /*is_customized=*/true);
          }
        }
        resolved->tab_strip->ActivateTabAt(resolved->index);
        if (resolved->browser && resolved->browser->GetWindow()) {
          resolved->browser->GetWindow()->Activate();
        }
        base::DictValue taken;
        taken.Set("type", "activate_tab");
        taken.Set("tab_id", *tid);
        taken.Set("title", base::UTF16ToUTF8(resolved->contents->GetTitle()));
        actions_taken.Append(std::move(taken));
      } else if (*type == "pin_tabs" || *type == "unpin_tabs") {
        const bool should_pin = (*type == "pin_tabs");
        const base::ListValue* tab_ids = action.FindList("tab_ids");
        if (!tab_ids || tab_ids->empty()) {
          continue;
        }
        int changed_count = 0;
        for (const base::Value& tid_val : *tab_ids) {
          std::optional<int> tid = tid_val.GetIfInt();
          if (!tid.has_value()) {
            continue;
          }
          std::optional<ResolvedWorkspaceTab> resolved =
              ResolveWorkspaceTabById(browser_, *tid);
          if (!resolved.has_value() || !resolved->tab_strip) {
            base::DictValue skip_entry;
            skip_entry.Set("tab_id", *tid);
            skip_entry.Set("reason", "not_found");
            skipped.Append(std::move(skip_entry));
            continue;
          }
          resolved->tab_strip->SetTabPinned(resolved->index, should_pin);
          ++changed_count;
        }
        if (changed_count > 0) {
          base::DictValue taken;
          taken.Set("type", *type);
          taken.Set("tab_count", changed_count);
          actions_taken.Append(std::move(taken));
        }
      } else if (*type == "mute_tabs" || *type == "unmute_tabs") {
        const bool should_mute = (*type == "mute_tabs");
        const base::ListValue* tab_ids = action.FindList("tab_ids");
        if (!tab_ids || tab_ids->empty()) {
          continue;
        }
        int changed_count = 0;
        for (const base::Value& tid_val : *tab_ids) {
          std::optional<int> tid = tid_val.GetIfInt();
          if (!tid.has_value()) {
            continue;
          }
          std::optional<ResolvedWorkspaceTab> resolved =
              ResolveWorkspaceTabById(browser_, *tid);
          if (!resolved.has_value() || !resolved->contents) {
            base::DictValue skip_entry;
            skip_entry.Set("tab_id", *tid);
            skip_entry.Set("reason", "not_found");
            skipped.Append(std::move(skip_entry));
            continue;
          }
          resolved->contents->SetAudioMuted(should_mute);
          ++changed_count;
        }
        if (changed_count > 0) {
          base::DictValue taken;
          taken.Set("type", *type);
          taken.Set("tab_count", changed_count);
          actions_taken.Append(std::move(taken));
        }
      } else if (*type == "discard_tabs") {
        const base::ListValue* tab_ids = action.FindList("tab_ids");
        if (!tab_ids || tab_ids->empty()) {
          continue;
        }
        int discarded_count = 0;
        for (const base::Value& tid_val : *tab_ids) {
          std::optional<int> tid = tid_val.GetIfInt();
          if (!tid.has_value()) {
            continue;
          }
          std::optional<ResolvedWorkspaceTab> resolved =
              ResolveWorkspaceTabById(browser_, *tid);
          if (!resolved.has_value() || !resolved->tab || !resolved->browser) {
            base::DictValue skip_entry;
            skip_entry.Set("tab_id", *tid);
            skip_entry.Set("reason", "not_found");
            skipped.Append(std::move(skip_entry));
            continue;
          }
          if (resolved->tab_strip &&
              resolved->tab_strip->active_index() == resolved->index) {
            base::DictValue skip_entry;
            skip_entry.Set("tab_id", *tid);
            skip_entry.Set("reason", "cannot_discard_active_tab");
            skipped.Append(std::move(skip_entry));
            continue;
          }
          if (WebContentsHasUnsavedState(resolved->contents)) {
            base::DictValue skip_entry;
            skip_entry.Set("tab_id", *tid);
            skip_entry.Set("reason", "has_unsaved_state");
            skipped.Append(std::move(skip_entry));
            continue;
          }
          TabListInterface* tab_list =
              TabListInterface::From(resolved->browser.get());
          if (tab_list && tab_list->DiscardTab(resolved->tab->GetHandle())) {
            ++discarded_count;
          } else {
            base::DictValue skip_entry;
            skip_entry.Set("tab_id", *tid);
            skip_entry.Set("reason", "discard_failed");
            skipped.Append(std::move(skip_entry));
          }
        }
        if (discarded_count > 0) {
          base::DictValue taken;
          taken.Set("type", "discard_tabs");
          taken.Set("tab_count", discarded_count);
          actions_taken.Append(std::move(taken));
        }
      } else if (*type == "open_saved_group") {
        std::string target_guid = action.FindString("saved_guid")
                                      ? *action.FindString("saved_guid")
                                      : "";
        std::string target_title =
            action.FindString("title")
                ? *action.FindString("title")
                : (action.FindString("group_title")
                       ? *action.FindString("group_title")
                       : "");
        Profile* profile = browser_->GetProfile();
        tab_groups::TabGroupSyncService* sync_service =
            profile
                ? tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile)
                : nullptr;
        if (!sync_service) {
          continue;
        }
        std::optional<tab_groups::SavedTabGroup> matched_group;
        for (const tab_groups::SavedTabGroup& group :
             sync_service->GetAllGroups()) {
          if (!target_guid.empty() &&
              base::EqualsCaseInsensitiveASCII(
                  group.saved_guid().AsLowercaseString(), target_guid)) {
            matched_group = group;
            break;
          }
          if (!target_title.empty() &&
              base::EqualsCaseInsensitiveASCII(base::UTF16ToUTF8(group.title()),
                                               target_title)) {
            matched_group = group;
            break;
          }
        }
        if (!matched_group.has_value()) {
          base::DictValue skip_entry;
          skip_entry.Set("type", "open_saved_group");
          skip_entry.Set("reason", "not_found");
          skipped.Append(std::move(skip_entry));
          continue;
        }
        sync_service->OpenTabGroup(
            matched_group->saved_guid(),
            std::make_unique<tab_groups::TabGroupActionContextDesktop>(
                browser_, tab_groups::OpeningSource::kUnknown));
        base::DictValue taken;
        taken.Set("type", "open_saved_group");
        taken.Set("title", base::UTF16ToUTF8(matched_group->title()));
        actions_taken.Append(std::move(taken));
      } else if (*type == "reorder_tabs") {
        const base::ListValue* tab_ids = action.FindList("tab_ids");
        if (!tab_ids || tab_ids->empty()) {
          continue;
        }

        // Map tabs to their respective TabStripModel and resolve indices.
        std::map<TabStripModel*, std::vector<int>> tabs_by_strip;
        for (const base::Value& tid_val : *tab_ids) {
          std::optional<int> tid = tid_val.GetIfInt();
          if (!tid.has_value()) {
            continue;
          }
          std::optional<ResolvedWorkspaceTab> resolved =
              ResolveWorkspaceTabById(browser_, *tid);
          if (!resolved.has_value()) {
            base::DictValue skip_entry;
            skip_entry.Set("tab_id", *tid);
            skip_entry.Set("reason", "not_found");
            skipped.Append(std::move(skip_entry));
            continue;
          }
          tabs_by_strip[resolved->tab_strip].push_back(*tid);
        }

        int total_reordered = 0;
        for (auto& [tab_strip, target_order_tids] : tabs_by_strip) {
          if (!tab_strip || target_order_tids.size() < 2) {
            continue;
          }

          // Find the minimum index occupied by these tabs as the target start
          // position.
          int min_index = tab_strip->count();
          for (int tid : target_order_tids) {
            std::optional<ResolvedWorkspaceTab> resolved =
                ResolveWorkspaceTabById(browser_, tid);
            if (resolved.has_value() && resolved->tab_strip == tab_strip) {
              min_index = std::min(min_index, resolved->index);
            }
          }
          if (min_index >= tab_strip->count()) {
            continue;
          }

          // Move each tab in target_order_tids sequentially starting from
          // min_index, preserving the tab's current group membership.
          int target_slot = min_index;
          for (int tid : target_order_tids) {
            std::optional<ResolvedWorkspaceTab> resolved =
                ResolveWorkspaceTabById(browser_, tid);
            if (!resolved.has_value() || resolved->tab_strip != tab_strip) {
              continue;
            }
            int current_slot = resolved->index;
            if (current_slot != target_slot) {
              tab_strip->MoveWebContentsAt(current_slot, target_slot,
                                           /*select_after_move=*/false);
              ++total_reordered;
            }
            ++target_slot;
          }
        }

        if (total_reordered > 0) {
          base::DictValue taken;
          taken.Set("type", "reorder_tabs");
          taken.Set("tab_count", static_cast<int>(tab_ids->size()));
          actions_taken.Append(std::move(taken));
        }
      } else if (*type == "move_to_new_window") {
        const base::ListValue* tab_ids = action.FindList("tab_ids");
        if (!tab_ids || tab_ids->empty()) {
          continue;
        }
        std::string group_title =
            action.FindString("group_title")
                ? *action.FindString("group_title")
                : (action.FindString("title") ? *action.FindString("title")
                                              : "");
        std::string color_str =
            action.FindString("color") ? *action.FindString("color") : "blue";

        std::map<BrowserWindowInterface*, std::vector<std::pair<int, int>>>
            to_move_by_browser;
        for (const base::Value& tid_val : *tab_ids) {
          std::optional<int> tid = tid_val.GetIfInt();
          if (!tid.has_value()) {
            continue;
          }
          std::optional<ResolvedWorkspaceTab> resolved =
              ResolveWorkspaceTabById(browser_, *tid);
          if (!resolved.has_value()) {
            base::DictValue skip_entry;
            skip_entry.Set("tab_id", *tid);
            skip_entry.Set("reason", "not_found");
            skipped.Append(std::move(skip_entry));
            continue;
          }
          to_move_by_browser[resolved->browser.get()].emplace_back(
              resolved->index, *tid);
        }

        std::map<BrowserWindowInterface*, std::vector<int>> indices_by_browser;
        for (auto& [src_bwi, pairs] : to_move_by_browser) {
          if (!src_bwi || pairs.empty() || !src_bwi->GetTabStripModel()) {
            continue;
          }
          std::sort(
              pairs.begin(), pairs.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
          pairs.erase(std::unique(pairs.begin(), pairs.end(),
                                  [](const auto& a, const auto& b) {
                                    return a.first == b.first;
                                  }),
                      pairs.end());

          // Safety guardrail: Never move all tabs out of a source window into a
          // new window (which would empty and close the source window).
          const size_t total_in_src =
              static_cast<size_t>(src_bwi->GetTabStripModel()->count());
          while (pairs.size() >= total_in_src && !pairs.empty()) {
            base::DictValue skip_entry;
            skip_entry.Set("tab_id", pairs.back().second);
            skip_entry.Set("reason", "cannot_move_all_tabs_from_window");
            skipped.Append(std::move(skip_entry));
            pairs.pop_back();
          }

          for (const auto& [idx, tid] : pairs) {
            indices_by_browser[src_bwi].push_back(idx);
          }
        }

        if (indices_by_browser.empty()) {
          continue;
        }

        Profile* profile = browser_->GetProfile();
        if (!profile) {
          continue;
        }
        BrowserWindowInterface* new_browser =
            CreateBrowserWindow(BrowserWindowCreateParams(*profile, true));
        if (!new_browser) {
          continue;
        }

        int total_moved = 0;
        for (auto& [src_bwi, indices] : indices_by_browser) {
          if (!src_bwi || indices.empty()) {
            continue;
          }
          chrome::MoveTabsToExistingWindow(src_bwi, new_browser, indices);
          total_moved += static_cast<int>(indices.size());
        }

        if (total_moved > 0 && !group_title.empty() &&
            new_browser->GetTabStripModel() &&
            new_browser->GetTabStripModel()->SupportsTabGroups()) {
          TabStripModel* new_strip = new_browser->GetTabStripModel();
          std::optional<tab_groups::TabGroupId> gid;
          if (new_strip->group_model() &&
              !new_strip->group_model()->ListTabGroups().empty()) {
            gid = new_strip->group_model()->ListTabGroups().front();
          } else {
            std::vector<int> all_indices(new_strip->count());
            std::iota(all_indices.begin(), all_indices.end(), 0);
            gid = new_strip->AddToNewGroup(all_indices);
          }
          if (gid.has_value()) {
            tab_groups::TabGroupVisualData visual_data(
                base::UTF8ToUTF16(group_title),
                StringToTabGroupColor(color_str),
                /*is_collapsed=*/false);
            new_strip->ChangeTabGroupVisuals(*gid, std::move(visual_data),
                                             /*is_customized=*/true);
          }
        }

        if (new_browser->GetWindow()) {
          new_browser->GetWindow()->Show();
        }

        base::DictValue taken;
        taken.Set("type", "move_to_new_window");
        taken.Set("tab_count", total_moved);
        if (!group_title.empty()) {
          taken.Set("group_title", group_title);
        }
        actions_taken.Append(std::move(taken));
      } else if (*type == "move_tabs_to_window") {
        const base::ListValue* tab_ids = action.FindList("tab_ids");
        std::optional<int> target_win_idx = action.FindInt("target_window");
        if (!target_win_idx.has_value()) {
          target_win_idx = action.FindInt("window_index");
        }
        if (!target_win_idx.has_value() || !tab_ids || tab_ids->empty()) {
          continue;
        }

        BrowserWindowInterface* target_browser = ResolveBrowserWindowByIndex(
            browser_, batch_window_session_ids, *target_win_idx);
        if (!target_browser) {
          base::DictValue skip_entry;
          skip_entry.Set("type", "move_tabs_to_window");
          skip_entry.Set("target_window", *target_win_idx);
          skip_entry.Set("reason", "target_window_not_found");
          skipped.Append(std::move(skip_entry));
          continue;
        }

        std::map<BrowserWindowInterface*, std::vector<std::pair<int, int>>>
            to_move_by_browser;
        for (const base::Value& tid_val : *tab_ids) {
          std::optional<int> tid = tid_val.GetIfInt();
          if (!tid.has_value()) {
            continue;
          }
          std::optional<ResolvedWorkspaceTab> resolved =
              ResolveWorkspaceTabById(browser_, *tid);
          if (!resolved.has_value()) {
            base::DictValue skip_entry;
            skip_entry.Set("tab_id", *tid);
            skip_entry.Set("reason", "not_found");
            skipped.Append(std::move(skip_entry));
            continue;
          }
          if (resolved->browser.get() == target_browser) {
            continue;
          }
          to_move_by_browser[resolved->browser.get()].emplace_back(
              resolved->index, *tid);
        }

        int total_moved = 0;
        for (auto& [src_bwi, pairs] : to_move_by_browser) {
          if (!src_bwi || pairs.empty() || !src_bwi->GetTabStripModel()) {
            continue;
          }
          std::sort(
              pairs.begin(), pairs.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });
          pairs.erase(std::unique(pairs.begin(), pairs.end(),
                                  [](const auto& a, const auto& b) {
                                    return a.first == b.first;
                                  }),
                      pairs.end());

          // Safety guardrail: Never empty `browser_` (which hosts the active AI
          // Overlay Dialog session).
          if (src_bwi == browser_) {
            const size_t total_in_src =
                static_cast<size_t>(src_bwi->GetTabStripModel()->count());
            while (pairs.size() >= total_in_src && !pairs.empty()) {
              base::DictValue skip_entry;
              skip_entry.Set("tab_id", pairs.back().second);
              skip_entry.Set("reason", "cannot_empty_active_overlay_window");
              skipped.Append(std::move(skip_entry));
              pairs.pop_back();
            }
          }
          if (pairs.empty()) {
            continue;
          }

          std::vector<int> indices;
          indices.reserve(pairs.size());
          for (const auto& [idx, tid] : pairs) {
            indices.push_back(idx);
          }
          chrome::MoveTabsToExistingWindow(src_bwi, target_browser, indices);
          total_moved += static_cast<int>(indices.size());
        }

        if (total_moved > 0) {
          if (target_browser->GetWindow()) {
            target_browser->GetWindow()->Show();
          }

          base::DictValue taken;
          taken.Set("type", "move_tabs_to_window");
          taken.Set("target_window", *target_win_idx);
          taken.Set("tab_count", total_moved);
          actions_taken.Append(std::move(taken));
        }
      } else if (*type == "merge_windows") {
        std::optional<int> src_win_idx = action.FindInt("source_window");
        std::optional<int> target_win_idx = action.FindInt("target_window");
        if (!src_win_idx.has_value() || !target_win_idx.has_value()) {
          continue;
        }

        BrowserWindowInterface* src_browser = ResolveBrowserWindowByIndex(
            browser_, batch_window_session_ids, *src_win_idx);
        BrowserWindowInterface* target_browser = ResolveBrowserWindowByIndex(
            browser_, batch_window_session_ids, *target_win_idx);
        if (!src_browser || !target_browser) {
          base::DictValue skip_entry;
          skip_entry.Set("type", "merge_windows");
          skip_entry.Set("reason", "window_not_found");
          skipped.Append(std::move(skip_entry));
          continue;
        }
        if (src_browser == target_browser) {
          base::DictValue skip_entry;
          skip_entry.Set("type", "merge_windows");
          skip_entry.Set("reason", "source_and_target_are_same_window");
          skipped.Append(std::move(skip_entry));
          continue;
        }
        // Safety guardrail: Never empty `browser_` (which hosts the active AI
        // Overlay Dialog session).
        if (src_browser == browser_) {
          base::DictValue skip_entry;
          skip_entry.Set("type", "merge_windows");
          skip_entry.Set("source_window", *src_win_idx);
          skip_entry.Set("target_window", *target_win_idx);
          skip_entry.Set("reason", "cannot_close_active_overlay_window");
          skipped.Append(std::move(skip_entry));
          continue;
        }

        TabStripModel* src_strip = src_browser->GetTabStripModel();
        if (!src_strip || src_strip->count() == 0) {
          continue;
        }
        std::vector<int> all_indices(src_strip->count());
        std::iota(all_indices.begin(), all_indices.end(), 0);
        int moved_count = static_cast<int>(all_indices.size());
        chrome::MoveTabsToExistingWindow(src_browser, target_browser,
                                         all_indices);

        if (target_browser->GetWindow()) {
          target_browser->GetWindow()->Show();
          target_browser->GetWindow()->Activate();
        }

        base::DictValue taken;
        taken.Set("type", "merge_windows");
        taken.Set("source_window", *src_win_idx);
        taken.Set("target_window", *target_win_idx);
        taken.Set("tab_count", moved_count);
        actions_taken.Append(std::move(taken));
      } else if (*type == "undo_last_action") {
        if (RestoreWorkspaceUndoSnapshot()) {
          base::DictValue taken;
          taken.Set("type", "undo_last_action");
          taken.Set("restored", true);
          actions_taken.Append(std::move(taken));
        } else {
          base::DictValue skip_entry;
          skip_entry.Set("type", "undo_last_action");
          skip_entry.Set("reason", "no_action_to_undo");
          skipped.Append(std::move(skip_entry));
        }
      }
    }

    // If a mutating action was requested but all targets were skipped, preserve
    // the previous undo snapshot instead of overwriting it with a no-op state.
    if (has_mutating_action && actions_taken.empty()) {
      workspace_undo_snapshot_ = std::move(prior_undo_snapshot);
    }
  }

  if (facts.empty() && !actions_taken.empty()) {
    facts = base::StringPrintf("Executed %zu workspace action(s).",
                               actions_taken.size());
  }

  DVLOG(1) << "AiOverlayTools::ExecuteWorkspaceSubAgentOutput: actions_taken="
           << actions_taken.size() << ", skipped=" << skipped.size()
           << ", facts=\"" << facts << "\"";

  base::DictValue out;
  out.Set("facts", facts);
  if (!actions_taken.empty()) {
    out.Set("actions_taken", std::move(actions_taken));
  }
  if (!skipped.empty()) {
    out.Set("skipped", std::move(skipped));
  }
  return out;
#else
  base::DictValue out;
  out.Set("facts", std::string(raw_output));
  return out;
#endif  // !BUILDFLAG(IS_ANDROID)
}

void AiOverlayTools::AnalyzeAndManageTabs(
    const std::string& request,
    AnalyzeAndManageTabsCallback callback) {
  RecordToolCallInvoked("AnalyzeAndManageTabs");
  if (!features::kAiOverlayDialogNext.Get()) {
    std::move(callback).Run(
        base::unexpected("Workspace tools are not enabled."));
    return;
  }
#if !BUILDFLAG(IS_ANDROID)
  if (!browser_) {
    std::move(callback).Run(base::unexpected("Browser window not available"));
    return;
  }
  Profile* profile = browser_->GetProfile();
  if (!profile) {
    std::move(callback).Run(base::unexpected("Profile not available"));
    return;
  }

  std::string_view cleaned_req = StripMarkdownJsonFences(request);
  if (base::StartsWith(cleaned_req, "{")) {
    std::move(callback).Run(
        base::ok(base::Value(ExecuteWorkspaceSubAgentOutput(cleaned_req))));
    return;
  }

  base::ElapsedTimer snapshot_timer;
  base::DictValue snapshot = BuildWorkspaceSnapshot();
  base::UmaHistogramTimes("AI.OverlayDialog.WorkspaceSubAgent.SnapshotDuration",
                          snapshot_timer.Elapsed());
  std::move(callback).Run(base::ok(base::Value(std::move(snapshot))));
#else
  std::move(callback).Run(base::unexpected("Not supported on Android"));
#endif  // !BUILDFLAG(IS_ANDROID)
}

}  // namespace ttc
