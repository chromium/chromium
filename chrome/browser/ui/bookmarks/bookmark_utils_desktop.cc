// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"

#include <iterator>
#include <numeric>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/url_and_id.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/incognito_allowed_url.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace bookmarks {

size_t kNumBookmarkUrlsBeforePrompting = 15;

static BookmarkNavigationWrapper* g_nav_wrapper_test_instance = nullptr;

base::WeakPtr<content::NavigationHandle> BookmarkNavigationWrapper::NavigateTo(
    NavigateParams* params) {
  return g_nav_wrapper_test_instance
             ? g_nav_wrapper_test_instance->NavigateTo(params)
             : Navigate(params);
}

// static
void BookmarkNavigationWrapper::SetInstanceForTesting(
    BookmarkNavigationWrapper* instance) {
  g_nav_wrapper_test_instance = instance;
}

namespace {
// Represents a reference set of web contents opened by OpenAllHelper() so that
// the actual web contents and what browsers they are located in can be
// determined (if necessary).
using OpenedWebContentsSet = base::flat_set<const content::WebContents*>;

// Result of user action when a dialog is shown.
// The dialog is shown when user tries to open a bookmark folder as tab group
// that already connected with one before.
enum class OpenGroupMessageBoxResult {
  // No UI shows, user does not need to make a choice. Default to create a new
  // group.
  kNoMessage = 0,

  // User chooses to create a new group.
  kCreateNewGroup = 1,

  // User chooses to replace the old group with bookmarks in the folder.
  kReplaceOldGroup = 2,

  // User presses OK button, still need to determine whether user has checked
  // the replace old group checkbox later.
  kUserConfirm = 3,

  // User presses cancel button. Do nothing.
  kUserCancel = 4,
};

// Opens all of the URLs in `bookmark_urls` using `navigator` and
// `initial_disposition` as a starting point. Returns a reference set of the
// WebContents created; see OpenedWebContentsSet.
OpenedWebContentsSet OpenAllHelper(
    Browser* browser,
    std::vector<UrlAndId> bookmark_urls,
    WindowOpenDisposition initial_disposition,
    page_load_metrics::NavigationHandleUserData::InitiatorLocation
        navigation_type,
    std::optional<BookmarkLaunchAction> launch_action) {
  OpenedWebContentsSet::container_type opened_tabs;
  WindowOpenDisposition disposition = initial_disposition;
  // We keep track of (potentially) two browsers in addition to the original
  // browser. This allows us to open the URLs in the correct
  // browser depending on the URL type and `initial_disposition`.
  Browser* regular_browser = nullptr;
  Browser* incognito_browser = nullptr;
  bookmarks::BookmarkNavigationWrapper nav_wrapper;
  Profile* profile = nullptr;
  if (browser) {
    profile = browser->profile();
  }
  bool opening_urls_in_incognito = false;
  if (profile) {
    opening_urls_in_incognito =
        profile->IsIncognitoProfile() ||
        initial_disposition == WindowOpenDisposition::OFF_THE_RECORD;
  } else {
    opening_urls_in_incognito =
        initial_disposition == WindowOpenDisposition::OFF_THE_RECORD;
  }

  for (const auto& bookmark_url : bookmark_urls) {
    const bool url_allowed_in_incognito =
        IsURLAllowedInIncognito(bookmark_url.url);

    // Set the browser from which the URL will be opened. If neither
    // `incognito_browser` nor `regular_browser` is set we use the original
    // browser, but `NavigateTo` can create a new browser
    // depending on the disposition and URL type.
    Browser* browser_to_use = browser;
    if (opening_urls_in_incognito && url_allowed_in_incognito) {
      if (incognito_browser) {
        browser_to_use = incognito_browser;
      }
    } else {
      if (regular_browser) {
        browser_to_use = regular_browser;
      }
    }
    if (browser_to_use) {
      profile = browser_to_use->profile();
    }
    NavigateParams params(profile, bookmark_url.url,
                          ui::PAGE_TRANSITION_AUTO_BOOKMARK);
    params.disposition = disposition;
    params.browser = browser_to_use;
    base::WeakPtr<content::NavigationHandle> handle =
        nav_wrapper.NavigateTo(&params);
    if (handle) {
      page_load_metrics::NavigationHandleUserData::CreateForNavigationHandle(
          *handle, navigation_type);
    }
    content::WebContents* opened_tab =
        handle ? handle->GetWebContents() : nullptr;
    if (!opened_tab) {
      continue;
    }

    if (launch_action.has_value()) {
      BookmarkStatsTabHelper::CreateForWebContents(opened_tab);
      BookmarkStatsTabHelper::FromWebContents(opened_tab)
          ->SetLaunchAction(launch_action.value(), disposition);
    }

    if (bookmark_url.id != -1) {
      ChromeNavigationUIData* ui_data =
          static_cast<ChromeNavigationUIData*>(handle->GetNavigationUIData());
      if (ui_data) {
        ui_data->set_bookmark_id(bookmark_url.id);
      }
    }

    bool opening_in_new_window =
        disposition == WindowOpenDisposition::NEW_WINDOW ||
        disposition == WindowOpenDisposition::OFF_THE_RECORD;
    // If we are opening URLs in a new window we set the disposition to
    // `NEW_BACKGROUND_TAB` so that the rest of the URLs open in a new tab
    // instead of a new window.
    // Exception is when we are opening URLs in new incognito window
    // there is a URL that is not allowed in incognito mode.
    // In this case we don't set the disposition to `NEW_BACKGROUND_TAB`
    // until we have opened the first URL that can be opened in incognito.
    // See crbug.com/1349283.
    if (opening_in_new_window) {
      if (!opening_urls_in_incognito || url_allowed_in_incognito) {
        disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
      }
    }

    // After we open a URL there can be a new browser created, depending on
    // the disposition and the URL type.
    Profile* new_tab_profile =
        Profile::FromBrowserContext(opened_tab->GetBrowserContext());
    if (new_tab_profile->IsIncognitoProfile()) {
      if (!incognito_browser) {
        incognito_browser = chrome::FindBrowserWithTab(opened_tab);
      }
    } else {
      if (!regular_browser) {
        regular_browser = chrome::FindBrowserWithTab(opened_tab);
      }
    }

    opened_tabs.push_back(opened_tab);
  }

  // Constructing the return value in this way is significantly more efficient.
  return OpenedWebContentsSet(std::move(opened_tabs));
}

// Returns a vector of all URLs in |nodes| and their immediate children.  Only
// recurses one level deep, not infinitely.  TODO(pkasting): It's not clear why
// this shouldn't just recurse infinitely.
std::vector<UrlAndId> GetURLsToOpen(
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>& nodes,
    bool incognito_urls_only = false) {
  std::vector<UrlAndId> url_and_ids;
  const auto AddUrlIfLegal = [&](const GURL url, int64_t id) {
    if (!incognito_urls_only || IsURLAllowedInIncognito(url)) {
      UrlAndId url_and_id;
      url_and_id.url = url;
      url_and_id.id = id;
      url_and_ids.push_back(url_and_id);
    }
  };
  for (const BookmarkNode* node : nodes) {
    if (node->is_url()) {
      AddUrlIfLegal(node->url(), node->id());
    } else {
      // If the node is not a URL, it is a folder. We want to add those of its
      // children which are URLs.
      for (const auto& child : node->children()) {
        if (child->is_url()) {
          AddUrlIfLegal(child->url(), child->id());
        }
      }
    }
  }
  return url_and_ids;
}

// Returns the total number of descendants nodes.
int ChildURLCountTotal(const BookmarkNode* node) {
  const auto count_children = [](int total, const auto& child) {
    if (child->is_folder()) {
      total += ChildURLCountTotal(child.get());
    }
    return total + 1;
  };
  return std::accumulate(node->children().cbegin(), node->children().cend(), 0,
                         count_children);
}

// Returns in |urls|, the url and title pairs for each open tab in browser.
void GetURLsAndFoldersForOpenTabs(
    Browser* browser,
    std::vector<BookmarkEditor::EditDetails::BookmarkData>* folder_data) {
  std::vector<std::pair<GURL, std::u16string>> tab_entries;
  base::flat_map<int, TabGroupData> groups_by_index;
  for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
    std::pair<GURL, std::u16string> entry;
    auto* contents = browser->tab_strip_model()->GetWebContentsAt(i);
    chrome::GetURLAndTitleToBookmark(contents, &(entry.first), &(entry.second));
    tab_entries.push_back(entry);
    auto tab_group_id = browser->tab_strip_model()->GetTabGroupForTab(i);
    std::u16string title;
    if (tab_group_id.has_value()) {
      title = browser->tab_strip_model()
                  ->group_model()
                  ->GetTabGroup(tab_group_id.value())
                  ->visual_data()
                  ->title();
    }
    groups_by_index.emplace(i, std::make_pair(tab_group_id, title));
  }
  GetURLsAndFoldersForTabEntries(folder_data, tab_entries, groups_by_index);
}

std::optional<base::Uuid> GetConnectedTabGroupIdFromBookmarkFolder(
    const tab_groups::TabGroupSyncService* tab_group_sync_service,
    std::optional<base::Uuid> bookmark_folder_id) {
  if (!tab_group_sync_service || !bookmark_folder_id.has_value()) {
    return std::nullopt;
  }

  std::vector<tab_groups::SavedTabGroup> saved_groups =
      tab_group_sync_service->GetAllGroups();
  auto it = std::find_if(saved_groups.begin(), saved_groups.end(),
                         [&](const tab_groups::SavedTabGroup& group) {
                           return group.bookmark_node_id().has_value() &&
                                  group.bookmark_node_id().value() ==
                                      bookmark_folder_id.value();
                         });

  if (it == saved_groups.end()) {
    return std::nullopt;
  }

  return it->saved_guid();
}

// Open a folder of bookmarks as tabs.
void DoOpen(Browser* browser,
            std::vector<UrlAndId> url_and_ids_to_open,
            WindowOpenDisposition initial_disposition,
            std::optional<base::Uuid> bookmark_folder_node_id,
            std::optional<std::u16string> folder_title,
            bool add_to_split,
            page_load_metrics::NavigationHandleUserData::InitiatorLocation
                navigation_type,
            std::optional<BookmarkLaunchAction> launch_action,
            OpenGroupMessageBoxResult result,
            ui::DialogModelDelegate* delegate) {
  if (result == OpenGroupMessageBoxResult::kUserCancel) {
    base::RecordAction(
        base::UserMetricsAction("BookmarkTabGroupConversion_UserSelectCancel"));
    return;
  }

  if (delegate) {
    result = delegate->dialog_model()
                     ->GetCheckboxByUniqueId(kBookmarkReplaceOldGroupCheckboxId)
                     ->is_checked()
                 ? OpenGroupMessageBoxResult::kReplaceOldGroup
                 : OpenGroupMessageBoxResult::kCreateNewGroup;
  }

  const auto opened_web_contents = OpenAllHelper(
      browser, std::move(url_and_ids_to_open), initial_disposition,
      navigation_type, std::move(launch_action));
  if (add_to_split && opened_web_contents.size() == 1) {
    TabStripModel* model = browser->tab_strip_model();
    auto* const single_web_contents = *(opened_web_contents.begin());
    const int opened_web_contents_index =
        model->GetIndexOfWebContents(single_web_contents);
    model->AddToNewSplit(
        {opened_web_contents_index}, split_tabs::SplitTabVisualData(),
        split_tabs::SplitTabCreatedSource::kBookmarkContextMenu);
  } else if (folder_title.has_value()) {
    TabStripModel* model = browser->tab_strip_model();

    // Figure out which tabs we actually opened in this browser that
    // aren't already in groups.
    std::vector<int> tab_indices;
    for (int i = 0; i < model->count(); ++i) {
      if (base::Contains(opened_web_contents, model->GetWebContentsAt(i)) &&
          !model->GetTabGroupForTab(i).has_value()) {
        tab_indices.push_back(i);
      }
    }

    if (tab_indices.empty()) {
      return;
    }

    tab_groups::TabGroupSyncService* tab_group_sync_service =
        tab_groups::TabGroupSyncServiceFactory::GetForProfile(
            browser->profile());

    std::optional<base::Uuid> connected_group_id =
        GetConnectedTabGroupIdFromBookmarkFolder(tab_group_sync_service,
                                                 bookmark_folder_node_id);
    bool is_new_group = true;
    if (features::IsBookmarkTabGroupConversionEnabled() &&
        connected_group_id.has_value()) {
      if (result == OpenGroupMessageBoxResult::kReplaceOldGroup) {
        is_new_group = false;
        base::RecordAction(base::UserMetricsAction(
            "BookmarkTabGroupConversion_UserSelectReplaceOldGroup"));
      } else if (result == OpenGroupMessageBoxResult::kCreateNewGroup) {
        base::RecordAction(base::UserMetricsAction(
            "BookmarkTabGroupConversion_UserSelectCreateNewGroup"));
      }
    }

    if (is_new_group) {
      // Create a new group and add the tabs.
      tab_groups::TabGroupId new_group_id = model->AddToNewGroup(tab_indices);

      // Use the bookmark folder's title as the group's title.
      // TODO(http://crbug.com/436846784): Suggest a new name for the new tab
      // group if there is already a tab group with the same name.
      TabGroup* group = model->group_model()->GetTabGroup(new_group_id);
      const tab_groups::TabGroupVisualData* current_visual_data =
          group->visual_data();
      tab_groups::TabGroupVisualData new_visual_data(
          SuggestUniqueTabGroupName(folder_title.value(),
                                    tab_group_sync_service),
          current_visual_data->color(), current_visual_data->is_collapsed());
      model->ChangeTabGroupVisuals(group->id(), new_visual_data);

      model->OpenTabGroupEditor(new_group_id);

      if (!tab_group_sync_service ||
          !features::IsBookmarkTabGroupConversionEnabled()) {
        return;
      }

      if (connected_group_id.has_value()) {
        // Disconnect from old group.
        tab_group_sync_service->UpdateBookmarkNodeId(connected_group_id.value(),
                                                     std::nullopt);
      }

      // Connect to new group.
      std::optional<tab_groups::SavedTabGroup> new_tab_group =
          tab_group_sync_service->GetGroup(new_group_id);
      if (new_tab_group.has_value()) {
        tab_group_sync_service->UpdateBookmarkNodeId(
            new_tab_group->saved_guid(), bookmark_folder_node_id);
      }
    } else {
      if (!tab_group_sync_service) {
        return;
      }

      // Open existing group and replace existing tabs with the new ones.
      std::optional<tab_groups::TabGroupId> existing_group_id =
          tab_group_sync_service->OpenTabGroup(
              connected_group_id.value(),
              std::make_unique<tab_groups::TabGroupActionContextDesktop>(
                  browser, tab_groups::OpeningSource::kConnectOnGroupShare));

      if (!existing_group_id.has_value()) {
        return;
      }

      gfx::Range range = model->group_model()
                             ->GetTabGroup(existing_group_id.value())
                             ->ListTabs();
      std::vector<content::WebContents*> existing_tabs_in_group;
      for (size_t index = range.start(); index < range.end(); index++) {
        existing_tabs_in_group.push_back(model->GetWebContentsAt(index));
      }
      model->AddToExistingGroup(tab_indices, existing_group_id.value());
      for (content::WebContents* existing_tab : existing_tabs_in_group) {
        model->CloseWebContentsAt(model->GetIndexOfWebContents(existing_tab),
                                  TabCloseTypes::CLOSE_NONE);
      }
    }
  }
}

void DoOpenPromptConfirm(
    Browser* browser,
    std::vector<UrlAndId> url_and_ids_to_open,
    WindowOpenDisposition initial_disposition,
    std::optional<base::Uuid> bookmark_folder_node_id,
    std::optional<std::u16string> folder_title,
    bool add_to_split,
    page_load_metrics::NavigationHandleUserData::InitiatorLocation
        navigation_type,
    std::optional<BookmarkLaunchAction> launch_action,
    chrome::MessageBoxResult result) {
  if (result != chrome::MESSAGE_BOX_RESULT_YES) {
    return;
  }

  tab_groups::TabGroupSyncService* tab_group_sync_service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(browser->profile());
  std::optional<base::Uuid> connected_group_id =
      GetConnectedTabGroupIdFromBookmarkFolder(tab_group_sync_service,
                                               bookmark_folder_node_id);
  if (features::IsBookmarkTabGroupConversionEnabled() &&
      folder_title.has_value() && connected_group_id.has_value()) {
      // Show UI dialog for user selection.
      std::unique_ptr<ui::DialogModelDelegate> delegate =
          std::make_unique<ui::DialogModelDelegate>();
      ui::DialogModelDelegate* delegate_ptr = delegate.get();
      auto on_ok = base::BindOnce(
          DoOpen, browser, url_and_ids_to_open, initial_disposition,
          bookmark_folder_node_id, folder_title, add_to_split, navigation_type,
          launch_action, OpenGroupMessageBoxResult::kUserConfirm, delegate_ptr);
      auto on_cancel = base::BindOnce(
          DoOpen, browser, std::move(url_and_ids_to_open), initial_disposition,
          bookmark_folder_node_id, folder_title, add_to_split, navigation_type,
          launch_action, OpenGroupMessageBoxResult::kUserCancel, delegate_ptr);

      auto dialog_model_builder = ui::DialogModel::Builder(std::move(delegate));
      dialog_model_builder.SetInternalName(kReplaceOrCreateGroupDialogName)
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_BOOKMARK_BAR_REPLACE_OR_CREATE_NEW_GROUP_TITLE))
          .AddParagraph(ui::DialogModelLabel(l10n_util::GetStringUTF16(
              IDS_BOOKMARK_BAR_ALREADY_CREATED_GROUP)))
          .AddCheckbox(kBookmarkReplaceOldGroupCheckboxId,
                       ui::DialogModelLabel(l10n_util::GetStringUTF16(
                           IDS_BOOKMARK_BAR_REPLACE_OLD_GROUP_BUTTON)))
          .AddOkButton(std::move(on_ok))
          .AddCancelButton(std::move(on_cancel));

      chrome::ShowBrowserModal(browser, dialog_model_builder.Build());
      base::RecordAction(base::UserMetricsAction(
          "BookmarkTabGroupConversion_ShowGroupAlreadyCreatedDialog"));
  } else {
    DoOpen(browser, std::move(url_and_ids_to_open), initial_disposition,
           bookmark_folder_node_id, folder_title, add_to_split, navigation_type,
           launch_action, OpenGroupMessageBoxResult::kNoMessage, nullptr);
  }
}

}  // namespace

void OpenAllIfAllowed(
    Browser* browser,
    const std::vector<
        raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>& nodes,
    WindowOpenDisposition initial_disposition,
    bookmarks::OpenAllBookmarksContext context,
    page_load_metrics::NavigationHandleUserData::InitiatorLocation
        navigation_type,
    std::optional<BookmarkLaunchAction> launch_action) {
  std::vector<UrlAndId> url_and_ids = GetURLsToOpen(
      nodes, initial_disposition == WindowOpenDisposition::OFF_THE_RECORD);

  std::optional<base::Uuid> bookmark_folder_node_id;
  if (nodes.size() == 1 && nodes[0]->is_folder()) {
    bookmark_folder_node_id = nodes[0]->uuid();
  }

  // Skip the prompt if there are few bookmarks.
  size_t child_count = url_and_ids.size();
  if (child_count < bookmarks::kNumBookmarkUrlsBeforePrompting) {
    DoOpenPromptConfirm(
        browser, std::move(url_and_ids), initial_disposition,
        bookmark_folder_node_id,
        context == bookmarks::OpenAllBookmarksContext::kInGroup
            ? std::optional<std::u16string>(nodes[0]->GetTitledUrlNodeTitle())
            : std::nullopt,
        context == bookmarks::OpenAllBookmarksContext::kInSplit,
        navigation_type, std::move(launch_action),
        chrome::MESSAGE_BOX_RESULT_YES);
    return;
  }

  // The callback passed contains the pointer |browser|. This is safe
  // since if |browser| is closed, the message box will be destroyed
  // before the user can answer "Yes".

  chrome::ShowQuestionMessageBoxAsync(
      browser->window()->GetNativeWindow(),
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      l10n_util::GetStringFUTF16(IDS_BOOKMARK_BAR_SHOULD_OPEN_ALL,
                                 base::NumberToString16(child_count)),
      base::BindOnce(
          DoOpenPromptConfirm, browser, std::move(url_and_ids),
          initial_disposition, bookmark_folder_node_id,
          context == bookmarks::OpenAllBookmarksContext::kInGroup
              ? std::optional<std::u16string>(nodes[0]->GetTitledUrlNodeTitle())
              : std::nullopt,
          context == bookmarks::OpenAllBookmarksContext::kInSplit,
          navigation_type, std::nullopt));
}

int OpenCount(const std::vector<raw_ptr<const bookmarks::BookmarkNode,
                                        VectorExperimental>>& nodes,
              content::BrowserContext* incognito_context) {
  return GetURLsToOpen(nodes, incognito_context != nullptr).size();
}

int OpenCount(const BookmarkNode* node,
              content::BrowserContext* incognito_context) {
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(node);
  return OpenCount(
      std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>{
          node},
      incognito_context);
}

bool ConfirmDeleteBookmarkNode(gfx::NativeWindow window,
                               const BookmarkNode* node) {
  DCHECK(node && node->is_folder() && !node->children().empty());
  return chrome::ShowQuestionMessageBoxSync(
             window, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
             l10n_util::GetPluralStringFUTF16(
                 IDS_BOOKMARK_EDITOR_CONFIRM_DELETE,
                 ChildURLCountTotal(node))) == chrome::MESSAGE_BOX_RESULT_YES;
}

void ShowBookmarkAllTabsDialog(Browser* browser) {
  Profile* profile = browser->profile();
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile);
  DCHECK(model && model->loaded());

  const BookmarkNode* parent = GetParentForNewNodes(model);
  BookmarkEditor::EditDetails details =
      BookmarkEditor::EditDetails::AddFolder(parent, parent->children().size());

  GetURLsAndFoldersForOpenTabs(browser, &(details.bookmark_data.children));
  DCHECK(!details.bookmark_data.children.empty());
  BookmarkEditor::Show(browser->window()->GetNativeWindow(), profile, details,
                       BookmarkEditor::SHOW_TREE,
                       base::BindOnce(
                           [](const Profile* profile) {
                             // We record the profile that invoked this option.
                             RecordBookmarksAdded(profile);
                           },
                           base::Unretained(profile)));
}

void ShowBookmarkTabGroupDialog(
    Browser* browser,
    const TabGroup& tab_group,
    base::OnceCallback<void(Browser*, const tab_groups::TabGroupId&)>
        on_save_callback) {
  Profile* profile = browser->profile();
  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile);
  DCHECK(model && model->loaded());

  const BookmarkNode* parent = GetParentForNewNodes(model);
  BookmarkEditor::EditDetails details =
      BookmarkEditor::EditDetails::TabGroupToFolder(
          parent, parent->children().size(), tab_group.visual_data()->title());

  GetURLsAndFoldersForTabGroup(browser, tab_group,
                               &(details.bookmark_data.children));
  DCHECK(!details.bookmark_data.children.empty());
  BookmarkEditor::Show(
      browser->window()->GetNativeWindow(), profile, details,
      BookmarkEditor::SHOW_TREE,
      base::BindOnce(
          [](Browser* browser, const tab_groups::TabGroupId& tab_group_id,
             base::OnceCallback<void(Browser*, const tab_groups::TabGroupId&)>
                 callback) {
            // We record the profile that invoked this option.
            RecordBookmarksAdded(browser->profile());
            base::RecordAction(base::UserMetricsAction(
                "BookmarkTabGroupConversion_ConvertToBookmarkConfirmed"));
            if (callback) {
              std::move(callback).Run(browser, tab_group_id);
            }
          },
          base::Unretained(browser), tab_group.id(),
          std::move(on_save_callback)));
  base::RecordAction(base::UserMetricsAction(
      "BookmarkTabGroupConversion_ConvertToBookmarkSelected"));
}

bool HasBookmarkURLs(
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>&
        selection) {
  return !GetURLsToOpen(selection).empty();
}

bool HasBookmarkURLsAllowedInIncognitoMode(
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>&
        selection) {
  return !GetURLsToOpen(selection, true).empty();
}

void GetURLsAndFoldersForTabEntries(
    std::vector<BookmarkEditor::EditDetails::BookmarkData>* folder_data,
    std::vector<std::pair<GURL, std::u16string>> tab_entries,
    base::flat_map<int, TabGroupData> groups_by_index) {
  std::optional<tab_groups::TabGroupId> current_group_id;
  for (size_t i = 0; i < tab_entries.size(); ++i) {
    std::pair<GURL, std::u16string> entry = tab_entries.at(i);
    if (entry.first.is_empty()) {
      continue;
    }
    BookmarkEditor::EditDetails::BookmarkData child;
    child.url = entry.first;
    child.title = entry.second;
    if (groups_by_index.at(i).first.has_value()) {
      if (current_group_id != groups_by_index.at(i).first.value()) {
        BookmarkEditor::EditDetails::BookmarkData tab_group;
        tab_group.title = groups_by_index.at(i).second;
        folder_data->push_back(tab_group);
        current_group_id = groups_by_index.at(i).first;
      }
      folder_data->back().children.push_back(child);
    } else {
      folder_data->push_back(child);
    }
  }
}

void GetURLsAndFoldersForTabGroup(
    const Browser* browser,
    const TabGroup& tab_group,
    std::vector<BookmarkEditor::EditDetails::BookmarkData>* folder_data) {
  TabStripModel* const tab_strip_model = browser->tab_strip_model();
  const gfx::Range tab_range = tab_group.ListTabs();

  for (size_t i = tab_range.start(); i < tab_range.end(); ++i) {
    content::WebContents* web_contents = tab_strip_model->GetWebContentsAt(i);
    GURL url;
    std::u16string title;
    chrome::GetURLAndTitleToBookmark(web_contents, &url, &title);

    BookmarkEditor::EditDetails::BookmarkData bookmark_data;
    bookmark_data.url = url;
    bookmark_data.title = title;
    folder_data->push_back(bookmark_data);
  }
}

std::u16string SuggestUniqueTabGroupName(
    std::u16string folder_title,
    const tab_groups::TabGroupSyncService* tab_group_sync_service) {
  if (!tab_group_sync_service) {
    return folder_title;
  }

  std::vector<tab_groups::SavedTabGroup> saved_groups =
      tab_group_sync_service->GetAllGroups();
  base::flat_set<std::u16string> existing_titles;
  for (const auto& group : saved_groups) {
    existing_titles.insert(group.title());
  }

  if (!base::Contains(existing_titles, folder_title)) {
    return folder_title;
  }

  constexpr int kMaxAttempts = 100;
  for (int i = 1; i < kMaxAttempts; ++i) {
    std::u16string new_title =
        folder_title + u" (" + base::NumberToString16(i) + u")";
    if (!base::Contains(existing_titles, new_title)) {
      return new_title;
    }
  }

  return folder_title + u" (" + base::NumberToString16(kMaxAttempts) + u")";
}

}  // namespace bookmarks
