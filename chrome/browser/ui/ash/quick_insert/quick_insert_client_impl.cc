// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/quick_insert/quick_insert_client_impl.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/lobster/lobster_controller.h"
#include "ash/lobster/lobster_entry_point_enums.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "ash/quick_insert/quick_insert_controller.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/quick_insert_web_paste_target.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/files/file_enumerator.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/files/drive_search_provider.h"
#include "chrome/browser/ash/app_list/search/files/file_search_provider.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_provider.h"
#include "chrome/browser/ash/app_list/search/ranking/ranker_manager.h"
#include "chrome/browser/ash/app_list/search/search_engine.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/input_method/editor_mediator_factory.h"
#include "chrome/browser/ash/lobster/lobster_service_provider.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/quick_insert/quick_insert_file_suggester.h"
#include "chrome/browser/ui/ash/quick_insert/quick_insert_thumbnail_loader.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_context.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_mode.h"
#include "chromeos/ash/components/editor_menu/public/cpp/preset_text_query.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/aura/window.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace {

constexpr std::u16string_view kAnnouncementViewName = u"Quick Insert";

// Returns an `AppListControllerDelegate` with empty methods. Used only for
// constructing search engine providers.
AppListControllerDelegate* GetEmptyAppListControllerDelegate() {
  class QuickInsertAppListControllerDelegate
      : public AppListControllerDelegate {
   public:
    QuickInsertAppListControllerDelegate() = default;
    ~QuickInsertAppListControllerDelegate() override = default;

    // AppListControllerDelegate overrides:
    void DismissView() override { NOTIMPLEMENTED_LOG_ONCE(); }
    aura::Window* GetAppListWindow() override {
      NOTIMPLEMENTED_LOG_ONCE();
      return nullptr;
    }
    int64_t GetAppListDisplayId() override {
      NOTIMPLEMENTED_LOG_ONCE();
      return 0;
    }
    bool IsAppPinned(const std::string& app_id) override {
      NOTIMPLEMENTED_LOG_ONCE();
      return false;
    }
    bool IsAppOpen(const std::string& app_id) const override {
      NOTIMPLEMENTED_LOG_ONCE();
      return false;
    }
    void PinApp(const std::string& app_id) override {
      NOTIMPLEMENTED_LOG_ONCE();
    }
    void UnpinApp(const std::string& app_id) override {
      NOTIMPLEMENTED_LOG_ONCE();
    }
    Pinnable GetPinnable(const std::string& app_id) override {
      NOTIMPLEMENTED_LOG_ONCE();
      return AppListControllerDelegate::NO_PIN;
    }
    void CreateNewWindow(bool incognito,
                         bool should_trigger_session_restore) override {
      NOTIMPLEMENTED_LOG_ONCE();
    }
    void OpenURL(Profile* profile,
                 const GURL& url,
                 ui::PageTransition transition,
                 WindowOpenDisposition disposition) override {
      NOTIMPLEMENTED_LOG_ONCE();
    }
  };

  static base::NoDestructor<QuickInsertAppListControllerDelegate> delegate;
  return delegate.get();
}

std::vector<ash::QuickInsertSearchResult>
CreateSearchResultsForRecentLocalImages(
    std::vector<QuickInsertFileSuggester::LocalFile> files) {
  return base::ToVector(files,
                        [](QuickInsertFileSuggester::LocalFile& file)
                            -> ash::QuickInsertSearchResult {
                          return ash::QuickInsertLocalFileResult(
                              std::move(file.title), std::move(file.path));
                        });
}

std::vector<ash::QuickInsertSearchResult>
CreateSearchResultsForRecentDriveFiles(
    std::vector<QuickInsertFileSuggester::DriveFile> files) {
  return base::ToVector(files,
                        [](QuickInsertFileSuggester::DriveFile& file)
                            -> ash::QuickInsertSearchResult {
                          return ash::QuickInsertDriveFileResult(
                              std::move(file.id), std::move(file.title),
                              std::move(file.url), file.local_path);
                        });
}

std::unique_ptr<app_list::SearchProvider> CreateDriveSearchProvider(
    Profile* profile) {
  return std::make_unique<app_list::DriveSearchProvider>(
      profile, /*should_filter_shared_files=*/false,
      /*should_filter_directories=*/true);
}

std::unique_ptr<app_list::SearchProvider> CreateFileSearchProvider(
    Profile* profile) {
  return std::make_unique<app_list::FileSearchProvider>(
      profile, base::FileEnumerator::FileType::FILES,
      std::vector<std::string>{".jpg", ".jpeg", ".png", ".gif", ".webp"});
}

std::vector<ash::QuickInsertSearchResult> ConvertSearchResults(
    std::vector<std::unique_ptr<ChromeSearchResult>> results) {
  std::vector<ash::QuickInsertSearchResult> quick_insert_results;
  quick_insert_results.reserve(results.size());

  for (const std::unique_ptr<ChromeSearchResult>& result : results) {
    CHECK(result);
  }

  std::ranges::sort(results, std::ranges::greater(),
                    [](const std::unique_ptr<ChromeSearchResult>& result) {
                      return result->relevance();
                    });

  for (const std::unique_ptr<ChromeSearchResult>& result : results) {
    switch (result->result_type()) {
      case ash::AppListSearchResultType::kOmnibox:
      case ash::AppListSearchResultType::kOpenTab: {
        if (result->metrics_type() == ash::OMNIBOX_URL_WHAT_YOU_TYPED) {
          continue;
        }

        if (std::optional<GURL> result_url = result->url();
            result_url.has_value()) {
          quick_insert_results.push_back(ash::QuickInsertBrowsingHistoryResult(
              *result_url, result->title(), result->icon().icon,
              result->best_match()));
        } else {
          quick_insert_results.push_back(ash::QuickInsertTextResult(
              result->title(), ash::QuickInsertTextResult::Source::kOmnibox));
        }
        break;
      }
      case ash::AppListSearchResultType::kFileSearch: {
        quick_insert_results.push_back(ash::QuickInsertLocalFileResult(
            result->title(), result->filePath(), result->best_match()));
        break;
      }
      case ash::AppListSearchResultType::kDriveSearch:
        quick_insert_results.push_back(ash::QuickInsertDriveFileResult(
            result->DriveId(), result->title(), *result->url(),
            result->filePath(), result->best_match()));
        break;
      default:
        LOG(DFATAL) << "Got unexpected search result type "
                    << static_cast<int>(result->result_type());
        break;
    }
  }

  return quick_insert_results;
}

ash::input_method::EditorMediator* GetEditorMediator(Profile* profile) {
  if (!chromeos::features::IsOrcaEnabled()) {
    return nullptr;
  }

  return ash::input_method::EditorMediatorFactory::GetInstance()->GetForProfile(
      profile);
}

std::vector<ash::QuickInsertSearchResult> GetEditorResultsFromEditorContext(
    const chromeos::editor_menu::EditorContext& editor_context) {
  std::vector<ash::QuickInsertSearchResult> results;
  for (const chromeos::editor_menu::PresetTextQuery& query :
       editor_context.preset_queries) {
    results.push_back(ash::QuickInsertEditorResult(
        ash::QuickInsertEditorResult::Mode::kRewrite, query.name,
        query.category, query.text_query_id));
  }
  return results;
}

}  // namespace

QuickInsertClientImpl::QuickInsertClientImpl(
    ash::QuickInsertController* controller,
    user_manager::UserManager* user_manager)
    : announcer_(kAnnouncementViewName), controller_(controller) {
  controller_->SetClient(this);

  // As `QuickInsertClientImpl` is initialised in
  // `ChromeBrowserMainExtraPartsAsh::PostProfileInit`, the user manager does
  // not notify us of the first user "change".
  ActiveUserChanged(user_manager->GetActiveUser());
  user_session_state_observation_.Observe(user_manager);
}

QuickInsertClientImpl::~QuickInsertClientImpl() {
  // Calling `QuickInsertController::SetClient` with null requires the old
  // client (this client) to be valid. This is fine as we have not started
  // destructing anything yet.
  controller_->SetClient(nullptr);
}

scoped_refptr<network::SharedURLLoaderFactory>
QuickInsertClientImpl::GetSharedURLLoaderFactory() {
  CHECK(profile_);
  return profile_->GetURLLoaderFactory();
}

void QuickInsertClientImpl::StartCrosSearch(
    const std::u16string& query,
    std::optional<ash::QuickInsertCategory> category,
    CrosSearchResultsCallback callback) {
  ranker_manager_->Start(query, {{.category = app_list::Category::kWeb},
                                 {.category = app_list::Category::kFiles}});
  if (!category.has_value()) {
    CHECK(search_engine_);
    search_engine_->StartSearch(
        query, app_list::SearchOptions(),
        base::BindRepeating(&QuickInsertClientImpl::OnCrosSearchResultsUpdated,
                            weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  switch (*category) {
    case ash::QuickInsertCategory::kEditorWrite:
    case ash::QuickInsertCategory::kEditorRewrite:
    case ash::QuickInsertCategory::kLobsterWithNoSelectedText:
    case ash::QuickInsertCategory::kLobsterWithSelectedText:
    case ash::QuickInsertCategory::kEmojisGifs:
    case ash::QuickInsertCategory::kEmojis:
    case ash::QuickInsertCategory::kGifs:
    case ash::QuickInsertCategory::kClipboard:
    case ash::QuickInsertCategory::kDatesTimes:
    case ash::QuickInsertCategory::kUnitsMaths:
      DLOG(FATAL) << "Unexpected category for StartCrosSearch: "
                  << static_cast<int>(*category);
      break;
    case ash::QuickInsertCategory::kLinks:
    case ash::QuickInsertCategory::kDriveFiles:
    case ash::QuickInsertCategory::kLocalFiles: {
      if (filtered_search_engine_ == nullptr ||
          current_filter_category_ != category) {
        filtered_search_engine_ =
            std::make_unique<app_list::SearchEngine>(profile_);
        filtered_search_engine_->AddProvider(
            CreateSearchProviderForCategory(*category));
        current_filter_category_ = category;
      }

      filtered_search_engine_->StartSearch(
          query, app_list::SearchOptions(),
          base::BindRepeating(
              &QuickInsertClientImpl::OnCrosSearchResultsUpdated,
              weak_factory_.GetWeakPtr(), std::move(callback)));
    } break;
  }
}

void QuickInsertClientImpl::OnCrosSearchResultsUpdated(
    QuickInsertClientImpl::CrosSearchResultsCallback callback,
    ash::AppListSearchResultType result_type,
    std::vector<std::unique_ptr<ChromeSearchResult>> results) {
  app_list::ResultsMap results_map;
  results_map[result_type] = std::move(results);
  if (ranker_manager_ != nullptr) {
    ranker_manager_->UpdateResultRanks(results_map, result_type);
  }
  callback.Run(result_type,
               ConvertSearchResults(std::move(results_map[result_type])));
}

void QuickInsertClientImpl::StopCrosQuery() {
  CHECK(search_engine_);
  search_engine_->StopQuery();
}

bool QuickInsertClientImpl::IsEligibleForEditor() {
  ash::input_method::EditorMediator* editor_mediator =
      GetEditorMediator(profile_);
  if (editor_mediator == nullptr) {
    return false;
  }

  return editor_mediator->GetEditorMode() !=
         chromeos::editor_menu::EditorMode::kHardBlocked;
}

QuickInsertClientImpl::ShowEditorCallback
QuickInsertClientImpl::CacheEditorContext() {
  ash::input_method::EditorMediator* editor_mediator =
      GetEditorMediator(profile_);
  if (editor_mediator == nullptr) {
    return {};
  }

  editor_mediator->CacheContext();

  chromeos::editor_menu::EditorMode editor_mode =
      editor_mediator->GetEditorMode();
  if (editor_mode == chromeos::editor_menu::EditorMode::kSoftBlocked ||
      editor_mode == chromeos::editor_menu::EditorMode::kHardBlocked) {
    return {};
  }

  return base::BindOnce(&QuickInsertClientImpl::ShowEditor,
                        weak_factory_.GetWeakPtr());
}

QuickInsertClientImpl::ShowLobsterCallback
QuickInsertClientImpl::CacheLobsterContext(
    ui::TextInputClient* text_input_client) {
  if (!ash::features::IsLobsterEnabled()) {
    return base::NullCallback();
  }

  ash::LobsterController* lobster_controller =
      ash::Shell::Get()->lobster_controller();
  LobsterService* lobster_service =
      LobsterServiceProvider::GetForProfile(profile_);

  if (lobster_controller == nullptr || lobster_service == nullptr) {
    return base::NullCallback();
  }

  lobster_trigger_ = lobster_controller->CreateTrigger(
      ash::LobsterEntryPoint::kQuickInsert, text_input_client);

  if (!lobster_trigger_) {
    return base::NullCallback();
  }

  return base::BindOnce(&QuickInsertClientImpl::ShowLobster,
                        weak_factory_.GetWeakPtr());
}

void QuickInsertClientImpl::GetSuggestedEditorResults(
    SuggestedEditorResultsCallback callback) {
  ash::input_method::EditorMediator* editor_mediator =
      GetEditorMediator(profile_);
  if (editor_mediator == nullptr ||
      editor_mediator->panel_manager() == nullptr) {
    std::move(callback).Run({});
    return;
  }

  chromeos::editor_menu::EditorMode editor_mode =
      editor_mediator->GetEditorMode();
  if (editor_mode == chromeos::editor_menu::EditorMode::kHardBlocked ||
      editor_mode == chromeos::editor_menu::EditorMode::kSoftBlocked) {
    std::move(callback).Run({});
    return;
  }

  editor_mediator->panel_manager()->GetEditorPanelContext(
      base::BindOnce(GetEditorResultsFromEditorContext)
          .Then(std::move(callback)));
}

void QuickInsertClientImpl::GetRecentLocalFileResults(
    size_t max_files,
    base::TimeDelta now_delta,
    RecentFilesCallback callback) {
  file_suggester_->GetRecentLocalImages(
      max_files, now_delta,
      base::BindOnce(CreateSearchResultsForRecentLocalImages)
          .Then(std::move(callback)));
}

void QuickInsertClientImpl::GetRecentDriveFileResults(
    size_t max_files,
    RecentFilesCallback callback) {
  file_suggester_->GetRecentDriveFiles(
      max_files, base::BindOnce(CreateSearchResultsForRecentDriveFiles)
                     .Then(std::move(callback)));
}

void QuickInsertClientImpl::FetchFileThumbnail(
    const base::FilePath& path,
    const gfx::Size& size,
    FetchFileThumbnailCallback callback) {
  CHECK(thumbnail_loader_);
  thumbnail_loader_->Load(path, size, std::move(callback));
}

// Forked from `ClipboardHistoryControllerDelegateImpl::Paste`.
std::optional<ash::QuickInsertWebPasteTarget>
QuickInsertClientImpl::GetWebPasteTarget() {
  std::unique_ptr<content::RenderWidgetHostIterator> widgets =
      content::RenderWidgetHost::GetRenderWidgetHosts();
  while (content::RenderWidgetHost* rwh = widgets->GetNextHost()) {
    content::RenderViewHost* rvh = content::RenderViewHost::From(rwh);
    if (rvh == nullptr) {
      continue;
    }

    content::WebContents* web_contents =
        content::WebContents::FromRenderViewHost(rvh);
    if (web_contents == nullptr) {
      continue;
    }
    if (web_contents->GetPrimaryMainFrame()->GetRenderViewHost() != rvh) {
      continue;
    }

    content::RenderFrameHost* focused_frame = web_contents->GetFocusedFrame();
    if (focused_frame == nullptr) {
      continue;
    }

    content::WebContents* focused_web_contents =
        content::WebContents::FromRenderFrameHost(focused_frame);
    if (focused_web_contents == nullptr) {
      continue;
    }

    gfx::NativeView window = focused_web_contents->GetContentNativeView();
    if (window == nullptr) {
      continue;
    }
    if (!window->HasFocus()) {
      continue;
    }

    return std::make_optional<ash::QuickInsertWebPasteTarget>(
        focused_web_contents->GetLastCommittedURL(),
        // SAFETY: Callers must call this synchronously as per the
        // documentation, so this `base::Unretained` is safe.
        base::BindOnce(&content::WebContents::Paste,
                       base::Unretained(focused_web_contents)));
  }

  return std::nullopt;
}

void QuickInsertClientImpl::Announce(std::u16string_view message) {
  announcer_.Announce(std::u16string(message));
}

void QuickInsertClientImpl::ActiveUserChanged(user_manager::User* active_user) {
  if (!active_user) {
    SetProfile(nullptr);
    return;
  }

  active_user->AddProfileCreatedObserver(
      base::BindOnce(&QuickInsertClientImpl::SetProfileByUser,
                     weak_factory_.GetWeakPtr(), active_user));
}

history::HistoryService* QuickInsertClientImpl::GetHistoryService() {
  return HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
}

favicon::FaviconService* QuickInsertClientImpl::GetFaviconService() {
  return FaviconServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
}

void QuickInsertClientImpl::SetProfileByUser(const user_manager::User* user) {
  Profile* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(user));
  SetProfile(profile);
}

void QuickInsertClientImpl::SetProfile(Profile* profile) {
  if (profile_ == profile) {
    return;
  }

  profile_ = profile;

  search_engine_ = std::make_unique<app_list::SearchEngine>(profile_);
  search_engine_->AddProvider(CreateOmniboxProvider(
      /*bookmarks=*/true, /*history=*/true, /*open_tabs=*/true));
  search_engine_->AddProvider(CreateFileSearchProvider(profile_));
  search_engine_->AddProvider(CreateDriveSearchProvider(profile_));
  filtered_search_engine_ = nullptr;
  current_filter_category_ = std::nullopt;

  ranker_manager_ = std::make_unique<app_list::RankerManager>(profile_);

  file_suggester_ = std::make_unique<QuickInsertFileSuggester>(profile_);
  thumbnail_loader_ = std::make_unique<QuickInsertThumbnailLoader>(profile_);

  if (controller_ != nullptr) {
    controller_->OnClientPrefsSet(profile == nullptr ? nullptr
                                                     : profile->GetPrefs());
  }
}

std::unique_ptr<app_list::SearchProvider>
QuickInsertClientImpl::CreateOmniboxProvider(bool bookmarks,
                                             bool history,
                                             bool open_tabs) {
  return std::make_unique<app_list::OmniboxProvider>(
      profile_, GetEmptyAppListControllerDelegate(),
      LauncherSearchProviderTypes(bookmarks, history, open_tabs));
}

std::unique_ptr<app_list::SearchProvider>
QuickInsertClientImpl::CreateSearchProviderForCategory(
    ash::QuickInsertCategory category) {
  switch (category) {
    case ash::QuickInsertCategory::kEditorWrite:
    case ash::QuickInsertCategory::kEditorRewrite:
    case ash::QuickInsertCategory::kLobsterWithNoSelectedText:
    case ash::QuickInsertCategory::kLobsterWithSelectedText:
    case ash::QuickInsertCategory::kEmojisGifs:
    case ash::QuickInsertCategory::kEmojis:
    case ash::QuickInsertCategory::kGifs:
    case ash::QuickInsertCategory::kClipboard:
    case ash::QuickInsertCategory::kDatesTimes:
    case ash::QuickInsertCategory::kUnitsMaths:
      DLOG(FATAL) << "Unexpected category for autocomplete: "
                  << static_cast<int>(category);
      return nullptr;
    case ash::QuickInsertCategory::kLinks:
      return CreateOmniboxProvider(/*bookmarks=*/true, /*history=*/true,
                                   /*open_tabs=*/true);
    case ash::QuickInsertCategory::kDriveFiles:
      return CreateDriveSearchProvider(profile_);
    case ash::QuickInsertCategory::kLocalFiles:
      return CreateFileSearchProvider(profile_);
  }
}

void QuickInsertClientImpl::ShowEditor(
    std::optional<std::string> preset_query_id,
    std::optional<std::string> freeform_text) {
  ash::input_method::EditorMediator* editor_mediator =
      GetEditorMediator(profile_);
  if (editor_mediator != nullptr) {
    editor_mediator->HandleTrigger(std::move(preset_query_id),
                                   std::move(freeform_text));
  }
}

void QuickInsertClientImpl::ShowLobster(std::optional<std::string> query) {
  if (lobster_trigger_ != nullptr) {
    lobster_trigger_->Fire(query);
  }
}

int QuickInsertClientImpl::LauncherSearchProviderTypes(bool bookmarks,
                                                       bool history,
                                                       bool open_tabs) {
  int providers = 0;

  if (bookmarks) {
    providers |= AutocompleteProvider::TYPE_BOOKMARK;
  }

  if (history) {
    providers |= AutocompleteProvider::TYPE_HISTORY_QUICK |
                 AutocompleteProvider::TYPE_HISTORY_URL |
                 AutocompleteProvider::TYPE_HISTORY_FUZZY |
                 AutocompleteProvider::TYPE_HISTORY_EMBEDDINGS;
  }

  if (open_tabs) {
    providers |= AutocompleteProvider::TYPE_OPEN_TAB;
  }

  return providers;
}
