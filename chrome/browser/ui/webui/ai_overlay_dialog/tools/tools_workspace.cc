// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
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
#include "content/public/browser/web_contents.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/performance_controls/tab_resource_usage_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace {

#if !BUILDFLAG(IS_ANDROID)
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
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

namespace ttc {

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

            int tab_seq_id = next_tab_seq_id++;

            base::DictValue tab_dict;
            tab_dict.Set("tab_id", tab_seq_id);
            tab_dict.Set("title", base::UTF16ToUTF8(contents->GetTitle()));
            tab_dict.Set("url", contents->GetVisibleURL().spec());
            tab_dict.Set("is_active", tab_strip->active_index() == i);

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
                         contents->NeedToFireBeforeUnloadOrUnloadEvents());
            tab_dict.Set("is_audible", contents->IsCurrentlyAudible());
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

void AiOverlayTools::GetWorkspaceSnapshot(
    GetWorkspaceSnapshotCallback callback) {
  RecordToolCallInvoked("GetWorkspaceSnapshot");
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
