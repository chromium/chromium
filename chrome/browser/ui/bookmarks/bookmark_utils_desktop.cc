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
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/url_and_id.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats.h"
#include "chrome/browser/ui/bookmarks/bookmark_stats_tab_helper.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace chrome {

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

// Returns a vector of all URLs in |nodes| and their immediate children.  Only
// recurses one level deep, not infinitely.  TODO(pkasting): It's not clear why
// this shouldn't just recurse infinitely.
std::vector<UrlAndId> GetURLsToOpen(
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>& nodes,
    content::BrowserContext* browser_context = nullptr,
    bool incognito_urls_only = false) {
  std::vector<UrlAndId> url_and_ids;
  const auto AddUrlIfLegal = [&](const GURL url, int64_t id) {
    if (!incognito_urls_only || IsURLAllowedInIncognito(url, browser_context)) {
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
        if (child->is_url())
          AddUrlIfLegal(child->url(), child->id());
      }
    }
  }
  return url_and_ids;
}

// Returns the total number of descendants nodes.
int ChildURLCountTotal(const BookmarkNode* node) {
  const auto count_children = [](int total, const auto& child) {
    if (child->is_folder())
      total += ChildURLCountTotal(child.get());
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
    GetURLAndTitleToBookmark(contents, &(entry.first), &(entry.second));
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

// Represents a reference set of web contents opened by OpenAllHelper() so that
// the actual web contents and what browsers they are located in can be
// determined (if necessary).
using OpenedWebContentsSet = base::flat_set<const content::WebContents*>;

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
  BookmarkNavigationWrapper nav_wrapper;
  Profile* profile = nullptr;
  if (browser)
    profile = browser->profile();
  bool opening_urls_in_incognito = false;
  if (profile) {
    opening_urls_in_incognito =
        profile->IsIncognitoProfile() ||
        initial_disposition == WindowOpenDisposition::OFF_THE_RECORD;
  } else {
    opening_urls_in_incognito =
        initial_disposition == WindowOpenDisposition::OFF_THE_RECORD;
  }

  for (std::vector<UrlAndId>::const_iterator url_and_id_it =
           bookmark_urls.begin();
       url_and_id_it != bookmark_urls.end(); ++url_and_id_it) {
    bool url_allowed_in_incognito =
        IsURLAllowedInIncognito(url_and_id_it->url, nullptr);

    // Set the browser from which the URL will be opened. If neither
    // `incognito_browser` nor `regular_browser` is set we use the original
    // browser, but `NavigateTo` can create a new browser
    // depending on the disposition and URL type.
    Browser* browser_to_use = browser;
    if (opening_urls_in_incognito && url_allowed_in_incognito) {
      if (incognito_browser)
        browser_to_use = incognito_browser;
    } else {
      if (regular_browser)
        browser_to_use = regular_browser;
    }
    if (browser_to_use)
      profile = browser_to_use->profile();
    NavigateParams params(profile, url_and_id_it->url,
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
    if (!opened_tab)
      continue;

    if (launch_action.has_value()) {
      BookmarkStatsTabHelper::CreateForWebContents(opened_tab);
      BookmarkStatsTabHelper::FromWebContents(opened_tab)
          ->SetLaunchAction(launch_action.value(), disposition);
    }

    if (url_and_id_it->id != -1) {
      ChromeNavigationUIData* ui_data =
          static_cast<ChromeNavigationUIData*>(handle->GetNavigationUIData());
      if (ui_data)
        ui_data->set_bookmark_id(url_and_id_it->id);
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

}  // namespace

void OpenAllIfAllowed(
    Browser* browser,
    const std::vector<
        raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>& nodes,
    WindowOpenDisposition initial_disposition,
    bool add_to_group,
    page_load_metrics::NavigationHandleUserData::InitiatorLocation
        navigation_type,
    std::optional<BookmarkLaunchAction> launch_action) {
  std::vector<UrlAndId> url_and_ids = GetURLsToOpen(
      nodes, browser->profile(),
      initial_disposition == WindowOpenDisposition::OFF_THE_RECORD);
  auto do_open = [](Browser* browser, std::vector<UrlAndId> url_and_ids_to_open,
                    WindowOpenDisposition initial_disposition,
                    std::optional<std::u16string> folder_title,
                    page_load_metrics::NavigationHandleUserData::
                        InitiatorLocation navigation_type,
                    std::optional<BookmarkLaunchAction> launch_action,
                    chrome::MessageBoxResult result) {
    if (result != chrome::MESSAGE_BOX_RESULT_YES)
      return;
    const auto opened_web_contents = OpenAllHelper(
        browser, std::move(url_and_ids_to_open), initial_disposition,
        navigation_type, std::move(launch_action));
    if (folder_title.has_value()) {
      TabStripModel* model = browser->tab_strip_model();

      // Figure out which tabs we actually opened in this browser that aren't
      // already in groups.
      std::vector<int> tab_indices;
      for (int i = 0; i < model->count(); ++i) {
        if (base::Contains(opened_web_contents, model->GetWebContentsAt(i)) &&
            !model->GetTabGroupForTab(i).has_value()) {
          tab_indices.push_back(i);
        }
      }

      if (tab_indices.empty())
        return;

      std::optional<tab_groups::TabGroupId> new_group_id =
          model->AddToNewGroup(tab_indices);
      if (!new_group_id.has_value())
        return;

      // Use the bookmark folder's title as the group's title.
      TabGroup* group = model->group_model()->GetTabGroup(new_group_id.value());
      const tab_groups::TabGroupVisualData* current_visual_data =
          group->visual_data();
      tab_groups::TabGroupVisualData new_visual_data(
          folder_title.value(), current_visual_data->color(),
          current_visual_data->is_collapsed());
      group->SetVisualData(new_visual_data);

      model->OpenTabGroupEditor(new_group_id.value());
    }
  };

  // Skip the prompt if there are few bookmarks.
  size_t child_count = url_and_ids.size();
  if (child_count < kNumBookmarkUrlsBeforePrompting) {
    do_open(browser, std::move(url_and_ids), initial_disposition,
            add_to_group ? std::optional<std::u16string>(
                               nodes[0]->GetTitledUrlNodeTitle())
                         : std::nullopt,
            navigation_type, std::move(launch_action),
            chrome::MESSAGE_BOX_RESULT_YES);
    return;
  }

  // The callback passed contains the pointer |browser|. This is safe
  // since if |browser| is closed, the message box will be destroyed
  // before the user can answer "Yes".

  ShowQuestionMessageBox(
      browser->window()->GetNativeWindow(),
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      l10n_util::GetStringFUTF16(IDS_BOOKMARK_BAR_SHOULD_OPEN_ALL,
                                 base::NumberToString16(child_count)),
      base::BindOnce(
          do_open, browser, std::move(url_and_ids), initial_disposition,
          add_to_group
              ? std::optional<std::u16string>(nodes[0]->GetTitledUrlNodeTitle())
              : std::nullopt,
          navigation_type, std::nullopt));
}

int OpenCount(gfx::NativeWindow parent,
              const std::vector<raw_ptr<const bookmarks::BookmarkNode,
                                        VectorExperimental>>& nodes,
              content::BrowserContext* incognito_context) {
  return GetURLsToOpen(nodes, incognito_context, incognito_context != nullptr)
      .size();
}

int OpenCount(gfx::NativeWindow parent,
              const BookmarkNode* node,
              content::BrowserContext* incognito_context) {
  std::vector<const BookmarkNode*> nodes;
  nodes.push_back(node);
  return OpenCount(
      parent,
      std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>{
          node},
      incognito_context);
}

bool ConfirmDeleteBookmarkNode(gfx::NativeWindow window,
                               const BookmarkNode* node) {
  DCHECK(node && node->is_folder() && !node->children().empty());
  return ShowQuestionMessageBoxSync(
             window, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
             l10n_util::GetPluralStringFUTF16(
                 IDS_BOOKMARK_EDITOR_CONFIRM_DELETE,
                 ChildURLCountTotal(node))) == MESSAGE_BOX_RESULT_YES;
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

bool HasBookmarkURLs(
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>&
        selection) {
  return !GetURLsToOpen(selection).empty();
}

bool HasBookmarkURLsAllowedInIncognitoMode(
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>&
        selection,
    content::BrowserContext* browser_context) {
  return !GetURLsToOpen(selection, browser_context, true).empty();
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

}  // namespace chrome
