// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PICKER_PICKER_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_PICKER_PICKER_CLIENT_IMPL_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_client.h"
#include "ash/public/cpp/picker/picker_web_paste_target.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/search/ranking/ranker_manager.h"
#include "chrome/browser/ash/input_method/editor_announcer.h"
#include "chrome/browser/ash/login/session/user_session_manager.h"
#include "chrome/browser/ui/ash/picker/picker_link_suggester.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class PrefService;
class Profile;
class ChromeSearchResult;
class PickerFileSuggester;
class PickerThumbnailLoader;

namespace app_list {
class SearchEngine;
class SearchProvider;
struct CategoryMetadata;
}

namespace ash {
class PickerController;
}

namespace aura {
class Window;
}

namespace user_manager {
class User;
}

// Implements the PickerClient used by Ash.
class PickerClientImpl
    : public ash::PickerClient,
      public user_manager::UserManager::UserSessionStateObserver {
 public:
  // Sets this instance as the client of `controller`.
  // Automatically unsets the client when this instance is destroyed.
  // `manager` needs to outlive this class.
  explicit PickerClientImpl(ash::PickerController* controller,
                            user_manager::UserManager* user_manager);
  PickerClientImpl(const PickerClientImpl&) = delete;
  PickerClientImpl& operator=(const PickerClientImpl&) = delete;
  ~PickerClientImpl() override;

  // ash::PickerClient:
  void StartCrosSearch(const std::u16string& query,
                       std::optional<ash::PickerCategory> category,
                       CrosSearchResultsCallback callback) override;
  void StopCrosQuery() override;
  bool IsEligibleForEditor() override;
  ShowEditorCallback CacheEditorContext() override;
  void GetSuggestedEditorResults(
      SuggestedEditorResultsCallback callback) override;
  void GetRecentLocalFileResults(size_t max_files,
                                 RecentFilesCallback callback) override;
  void GetRecentDriveFileResults(size_t max_files,
                                 RecentFilesCallback callback) override;
  void GetSuggestedLinkResults(size_t max_results,
                               SuggestedLinksCallback callback) override;
  bool IsFeatureAllowedForDogfood() override;
  void FetchFileThumbnail(const base::FilePath& path,
                          const gfx::Size& size,
                          FetchFileThumbnailCallback callback) override;
  PrefService* GetPrefs() override;
  std::optional<ash::PickerWebPasteTarget> GetWebPasteTarget() override;
  void Announce(std::u16string_view message) override;

  // user_manager::UserManager::UserSessionStateObserver:
  void ActiveUserChanged(user_manager::User* active_user) override;

  void set_ranker_manager_for_test(
      std::unique_ptr<app_list::RankerManager> ranker_manager) {
    ranker_manager_ = std::move(ranker_manager);
  }

  PickerLinkSuggester* get_link_suggester_for_test() {
    return link_suggester_.get();
  }

 private:
  // Implements `AppListControllerDelegate` with empty methods. Used only for
  // constructing search engine providers.
  class PickerAppListControllerDelegate : public AppListControllerDelegate {
   public:
    PickerAppListControllerDelegate();
    ~PickerAppListControllerDelegate() override;

    // AppListControllerDelegate overrides:
    void DismissView() override;
    aura::Window* GetAppListWindow() override;
    int64_t GetAppListDisplayId() override;
    bool IsAppPinned(const std::string& app_id) override;
    bool IsAppOpen(const std::string& app_id) const override;
    void PinApp(const std::string& app_id) override;
    void UnpinApp(const std::string& app_id) override;
    Pinnable GetPinnable(const std::string& app_id) override;
    void CreateNewWindow(bool incognito,
                         bool should_trigger_session_restore) override;
    void OpenURL(Profile* profile,
                 const GURL& url,
                 ui::PageTransition transition,
                 WindowOpenDisposition disposition) override;
  };

  void OnCrosSearchResultsUpdated(
      CrosSearchResultsCallback callback,
      ash::AppListSearchResultType result_type,
      std::vector<std::unique_ptr<ChromeSearchResult>> results);
  void SetProfileByUser(const user_manager::User* user);
  void SetProfile(Profile* profile);

  std::unique_ptr<app_list::SearchProvider>
  CreateOmniboxProvider(bool bookmarks, bool history, bool open_tabs);
  std::unique_ptr<app_list::SearchProvider> CreateSearchProviderForCategory(
      ash::PickerCategory category);

  void ShowEditor(std::optional<std::string> preset_query_id,
                  std::optional<std::string> freeform_text);

  ash::input_method::EditorLiveRegionAnnouncer announcer_;

  raw_ptr<ash::PickerController> controller_ = nullptr;
  raw_ptr<Profile> profile_ = nullptr;

  std::unique_ptr<app_list::SearchEngine> search_engine_;
  PickerAppListControllerDelegate app_list_controller_delegate_;

  // A dedicated cros search engine for filtered searches.
  std::unique_ptr<app_list::SearchEngine> filtered_search_engine_;
  std::optional<ash::PickerCategory> current_filter_category_;

  std::unique_ptr<app_list::RankerManager> ranker_manager_;
  std::vector<app_list::CategoryMetadata> ranker_categories_;

  std::unique_ptr<PickerFileSuggester> file_suggester_;
  std::unique_ptr<PickerLinkSuggester> link_suggester_;
  std::unique_ptr<PickerThumbnailLoader> thumbnail_loader_;

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::UserSessionStateObserver>
      user_session_state_observation_{this};

  base::WeakPtrFactory<PickerClientImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_PICKER_PICKER_CLIENT_IMPL_H_
