// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/picker/picker_client_impl.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/picker/picker_controller.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "ash/public/cpp/picker/picker_web_paste_target.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/files/file_enumerator.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/ranges/algorithm.h"
#include "base/ranges/functional.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/files/drive_search_provider.h"
#include "chrome/browser/ash/app_list/search/files/file_search_provider.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_lacros_provider.h"
#include "chrome/browser/ash/app_list/search/omnibox/omnibox_provider.h"
#include "chrome/browser/ash/app_list/search/ranking/ranker_manager.h"
#include "chrome/browser/ash/app_list/search/search_engine.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/input_method/editor_mediator_factory.h"
#include "chrome/browser/chromeos/launcher_search/search_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/picker/picker_file_suggester.h"
#include "chrome/browser/ui/ash/picker/picker_lacros_omnibox_search_provider.h"
#include "chrome/browser/ui/ash/picker/picker_link_suggester.h"
#include "chrome/browser/ui/ash/picker/picker_thumbnail_loader.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/components/editor_menu/public/cpp/preset_text_query.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/aura/window.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace ash {
enum class AppListSearchResultType;
}

namespace {

// TODO: b/345303965 - Finalize this string.
constexpr std::u16string_view kAnnouncementViewName = u"Picker";

bool IsSupportedLocalFileFormat(const base::FilePath& file_path) {
  for (std::string_view extension :
       {".jpg", ".jpeg", ".png", ".gif", ".webp"}) {
    if (file_path.MatchesFinalExtension(extension)) {
      return true;
    }
  }
  return false;
}

std::vector<ash::PickerSearchResult> CreateSearchResultsForRecentLocalImages(
    std::vector<PickerFileSuggester::LocalFile> files) {
  std::vector<ash::PickerSearchResult> results;
  results.reserve(files.size());
  for (PickerFileSuggester::LocalFile& file : files) {
    results.push_back(ash::PickerSearchResult::LocalFile(std::move(file.title),
                                                         std::move(file.path)));
  }
  return results;
}

std::vector<ash::PickerSearchResult> CreateSearchResultsForRecentDriveFiles(
    std::vector<PickerFileSuggester::DriveFile> files) {
  std::vector<ash::PickerSearchResult> results;
  results.reserve(files.size());
  for (PickerFileSuggester::DriveFile& file : files) {
    results.push_back(ash::PickerSearchResult::DriveFile(
        std::move(file.id), std::move(file.title), std::move(file.url),
        file.local_path));
  }
  return results;
}

std::unique_ptr<app_list::SearchProvider> CreateDriveSearchProvider(
    Profile* profile) {
  auto provider = std::make_unique<app_list::DriveSearchProvider>(
      profile, /*should_filter_shared_files=*/false,
      /*should_filter_directories=*/true);
  if (base::FeatureList::IsEnabled(ash::features::kPickerCloud)) {
    provider->SetQuerySource(
        drivefs::mojom::QueryParameters::QuerySource::kCloudOnly);
  }
  return provider;
}

std::unique_ptr<app_list::SearchProvider> CreateFileSearchProvider(
    Profile* profile) {
  return std::make_unique<app_list::FileSearchProvider>(
      profile, base::FileEnumerator::FileType::FILES);
}

std::vector<ash::PickerSearchResult> ConvertSearchResults(
    std::vector<std::unique_ptr<ChromeSearchResult>> results) {
  std::vector<ash::PickerSearchResult> picker_results;
  picker_results.reserve(results.size());

  for (const std::unique_ptr<ChromeSearchResult>& result : results) {
    CHECK(result);
  }

  base::ranges::sort(results, base::ranges::greater(),
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
          picker_results.push_back(ash::PickerSearchResult::BrowsingHistory(
              *result_url, result->title(), result->icon().icon,
              result->best_match()));
        } else {
          picker_results.push_back(ash::PickerSearchResult::Text(
              result->title(),
              ash::PickerSearchResult::TextData::Source::kOmnibox));
        }
        break;
      }
      case ash::AppListSearchResultType::kFileSearch: {
        // TODO: b/322926411 - Move this filtering to the search provider.
        if (IsSupportedLocalFileFormat(result->filePath())) {
          picker_results.push_back(ash::PickerSearchResult::LocalFile(
              result->title(), result->filePath(), result->best_match()));
        }
        break;
      }
      case ash::AppListSearchResultType::kDriveSearch:
        picker_results.push_back(ash::PickerSearchResult::DriveFile(
            result->DriveId(), result->title(), *result->url(),
            result->filePath(), result->best_match()));
        break;
      default:
        LOG(DFATAL) << "Got unexpected search result type "
                    << static_cast<int>(result->result_type());
        break;
    }
  }

  return picker_results;
}

ash::input_method::EditorMediator* GetEditorMediator(Profile* profile) {
  if (!chromeos::features::IsOrcaEnabled()) {
    return nullptr;
  }

  return ash::input_method::EditorMediatorFactory::GetInstance()->GetForProfile(
      profile);
}

// TODO: b/326847990 - Remove this once it's moved to mojom traits.
chromeos::editor_menu::PresetQueryCategory FromMojoPresetQueryCategory(
    const crosapi::mojom::EditorPanelPresetQueryCategory category) {
  using EditorPanelPresetQueryCategory =
      crosapi::mojom::EditorPanelPresetQueryCategory;
  using PresetQueryCategory = chromeos::editor_menu::PresetQueryCategory;

  switch (category) {
    case EditorPanelPresetQueryCategory::kUnknown:
      return PresetQueryCategory::kUnknown;
    case EditorPanelPresetQueryCategory::kShorten:
      return PresetQueryCategory::kShorten;
    case EditorPanelPresetQueryCategory::kElaborate:
      return PresetQueryCategory::kElaborate;
    case EditorPanelPresetQueryCategory::kRephrase:
      return PresetQueryCategory::kRephrase;
    case EditorPanelPresetQueryCategory::kFormalize:
      return PresetQueryCategory::kFormalize;
    case EditorPanelPresetQueryCategory::kEmojify:
      return PresetQueryCategory::kEmojify;
    case EditorPanelPresetQueryCategory::kProofread:
      return PresetQueryCategory::kProofread;
  }
}

std::vector<ash::PickerSearchResult> GetEditorResultsFromPanelContext(
    crosapi::mojom::EditorPanelContextPtr panel_context) {
  std::vector<ash::PickerSearchResult> results;
  for (const crosapi::mojom::EditorPanelPresetTextQueryPtr& query :
       panel_context->preset_text_queries) {
    results.push_back(ash::PickerSearchResult::Editor(
        ash::PickerSearchResult::EditorData::Mode::kRewrite,
        base::UTF8ToUTF16(query->name),
        FromMojoPresetQueryCategory(query->category), query->text_query_id));
  }
  return results;
}

app_list::CategoriesList CreateRankerCategories() {
  app_list::CategoriesList res({{.category = app_list::Category::kWeb},
                                {.category = app_list::Category::kFiles}});
  return res;
}

}  // namespace

PickerClientImpl::PickerClientImpl(ash::PickerController* controller,
                                   user_manager::UserManager* user_manager)
    : announcer_(kAnnouncementViewName), controller_(controller) {
  controller_->SetClient(this);

  // As `PickerClientImpl` is initialised in
  // `ChromeBrowserMainExtraPartsAsh::PostProfileInit`, the user manager does
  // not notify us of the first user "change".
  ActiveUserChanged(user_manager->GetActiveUser());
  user_session_state_observation_.Observe(user_manager);
}

PickerClientImpl::~PickerClientImpl() {
  // Calling `PickerController::SetClient` with null requires the old client
  // (this client) to be valid. This is fine as we have not started destructing
  // anything yet.
  controller_->SetClient(nullptr);
}

void PickerClientImpl::StartCrosSearch(
    const std::u16string& query,
    std::optional<ash::PickerCategory> category,
    CrosSearchResultsCallback callback) {
  ranker_categories_ = CreateRankerCategories();
  ranker_manager_->Start(query, ranker_categories_);
  if (!category.has_value()) {
    CHECK(search_engine_);
    search_engine_->StartSearch(
        query, app_list::SearchOptions(),
        base::BindRepeating(&PickerClientImpl::OnCrosSearchResultsUpdated,
                            weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  switch (*category) {
    case ash::PickerCategory::kEditorWrite:
    case ash::PickerCategory::kEditorRewrite:
    case ash::PickerCategory::kEmojisGifs:
    case ash::PickerCategory::kEmojis:
    case ash::PickerCategory::kClipboard:
    case ash::PickerCategory::kDatesTimes:
    case ash::PickerCategory::kUnitsMaths:
      DLOG(FATAL) << "Unexpected category for StartCrosSearch: "
                  << static_cast<int>(*category);
      break;
    case ash::PickerCategory::kLinks:
    case ash::PickerCategory::kDriveFiles:
    case ash::PickerCategory::kLocalFiles: {
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
          base::BindRepeating(&PickerClientImpl::OnCrosSearchResultsUpdated,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
    } break;
  }
}

void PickerClientImpl::OnCrosSearchResultsUpdated(
    PickerClientImpl::CrosSearchResultsCallback callback,
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

void PickerClientImpl::StopCrosQuery() {
  CHECK(search_engine_);
  search_engine_->StopQuery();
}

bool PickerClientImpl::IsEligibleForEditor() {
  ash::input_method::EditorMediator* editor_mediator =
      GetEditorMediator(profile_);
  if (editor_mediator == nullptr) {
    return false;
  }

  return editor_mediator->GetEditorMode() !=
         ash::input_method::EditorMode::kHardBlocked;
}

PickerClientImpl::ShowEditorCallback PickerClientImpl::CacheEditorContext() {
  ash::input_method::EditorMediator* editor_mediator =
      GetEditorMediator(profile_);
  if (editor_mediator == nullptr) {
    return {};
  }

  editor_mediator->CacheContext();

  ash::input_method::EditorMode editor_mode = editor_mediator->GetEditorMode();
  if (editor_mode == ash::input_method::EditorMode::kSoftBlocked ||
      editor_mode == ash::input_method::EditorMode::kHardBlocked) {
    return {};
  }

  return base::BindOnce(&PickerClientImpl::ShowEditor,
                        weak_factory_.GetWeakPtr());
}

void PickerClientImpl::GetSuggestedEditorResults(
    SuggestedEditorResultsCallback callback) {
  ash::input_method::EditorMediator* editor_mediator =
      GetEditorMediator(profile_);
  if (editor_mediator == nullptr ||
      editor_mediator->panel_manager() == nullptr) {
    std::move(callback).Run({});
    return;
  }

  ash::input_method::EditorMode editor_mode = editor_mediator->GetEditorMode();
  if (editor_mode == ash::input_method::EditorMode::kHardBlocked ||
      editor_mode == ash::input_method::EditorMode::kSoftBlocked) {
    std::move(callback).Run({});
    return;
  }

  editor_mediator->panel_manager()->GetEditorPanelContext(
      base::BindOnce(GetEditorResultsFromPanelContext)
          .Then(std::move(callback)));
}

void PickerClientImpl::GetRecentLocalFileResults(size_t max_files,
                                                 RecentFilesCallback callback) {
  file_suggester_->GetRecentLocalImages(
      max_files, base::BindOnce(CreateSearchResultsForRecentLocalImages)
                     .Then(std::move(callback)));
}

void PickerClientImpl::GetRecentDriveFileResults(size_t max_files,
                                                 RecentFilesCallback callback) {
  file_suggester_->GetRecentDriveFiles(
      max_files, base::BindOnce(CreateSearchResultsForRecentDriveFiles)
                     .Then(std::move(callback)));
}

void PickerClientImpl::GetSuggestedLinkResults(
    size_t max_results,
    SuggestedLinksCallback callback) {
  link_suggester_->GetSuggestedLinks(max_results, std::move(callback));
}

bool PickerClientImpl::IsFeatureAllowedForDogfood() {
  return gaia::IsGoogleInternalAccountEmail(profile_->GetProfileUserName());
}

void PickerClientImpl::FetchFileThumbnail(const base::FilePath& path,
                                          const gfx::Size& size,
                                          FetchFileThumbnailCallback callback) {
  CHECK(thumbnail_loader_);
  thumbnail_loader_->Load(path, size, std::move(callback));
}

PrefService* PickerClientImpl::GetPrefs() {
  return profile_ == nullptr ? nullptr : profile_->GetPrefs();
}

// Forked from `ClipboardHistoryControllerDelegateImpl::Paste`.
std::optional<ash::PickerWebPasteTarget> PickerClientImpl::GetWebPasteTarget() {
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

    return std::make_optional<ash::PickerWebPasteTarget>(
        focused_web_contents->GetLastCommittedURL(),
        // SAFETY: Callers must call this synchronously as per the
        // documentation, so this `base::Unretained` is safe.
        base::BindOnce(&content::WebContents::Paste,
                       base::Unretained(focused_web_contents)));
  }

  return std::nullopt;
}

void PickerClientImpl::Announce(std::u16string_view message) {
  announcer_.Announce(std::u16string(message));
}

void PickerClientImpl::ActiveUserChanged(user_manager::User* active_user) {
  if (!active_user) {
    SetProfile(nullptr);
    return;
  }

  active_user->AddProfileCreatedObserver(
      base::BindOnce(&PickerClientImpl::SetProfileByUser,
                     weak_factory_.GetWeakPtr(), active_user));
}

void PickerClientImpl::SetProfileByUser(const user_manager::User* user) {
  Profile* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(user));
  SetProfile(profile);
}

void PickerClientImpl::SetProfile(Profile* profile) {
  if (profile_ == profile) {
    return;
  }

  profile_ = profile;

  search_engine_ = std::make_unique<app_list::SearchEngine>(profile_);
  search_engine_->AddProvider(CreateOmniboxProvider(
      /*bookmarks=*/true, /*history=*/true, /*open_tabs=*/true));
  search_engine_->AddProvider(CreateFileSearchProvider(profile_));
  search_engine_->AddProvider(CreateDriveSearchProvider(profile_));

  ranker_manager_ = std::make_unique<app_list::RankerManager>(profile_);

  file_suggester_ = std::make_unique<PickerFileSuggester>(profile_);
  link_suggester_ = std::make_unique<PickerLinkSuggester>(profile_);
  thumbnail_loader_ = std::make_unique<PickerThumbnailLoader>(profile_);

  if (controller_ != nullptr) {
    controller_->OnClientProfileSet();
  }
}

std::unique_ptr<app_list::SearchProvider>
PickerClientImpl::CreateOmniboxProvider(bool bookmarks,
                                        bool history,
                                        bool open_tabs) {
  if (crosapi::browser_util::IsLacrosEnabled()) {
    return std::make_unique<app_list::OmniboxLacrosProvider>(
        profile_, &app_list_controller_delegate_,
        PickerLacrosOmniboxSearchProvider::CreateControllerCallback(
            bookmarks, history, open_tabs));
  } else {
    return std::make_unique<app_list::OmniboxProvider>(
        profile_, &app_list_controller_delegate_,
        crosapi::ProviderTypesPicker(bookmarks, history, open_tabs));
  }
}

std::unique_ptr<app_list::SearchProvider>
PickerClientImpl::CreateSearchProviderForCategory(
    ash::PickerCategory category) {
  switch (category) {
    case ash::PickerCategory::kEditorWrite:
    case ash::PickerCategory::kEditorRewrite:
    case ash::PickerCategory::kEmojisGifs:
    case ash::PickerCategory::kEmojis:
    case ash::PickerCategory::kClipboard:
    case ash::PickerCategory::kDatesTimes:
    case ash::PickerCategory::kUnitsMaths:
      DLOG(FATAL) << "Unexpected category for autocomplete: "
                  << static_cast<int>(category);
      return nullptr;
    case ash::PickerCategory::kLinks:
      return CreateOmniboxProvider(/*bookmarks=*/true, /*history=*/true,
                                   /*open_tabs=*/true);
    case ash::PickerCategory::kDriveFiles:
      return CreateDriveSearchProvider(profile_);
    case ash::PickerCategory::kLocalFiles:
      return CreateFileSearchProvider(profile_);
  }
}

void PickerClientImpl::ShowEditor(std::optional<std::string> preset_query_id,
                                  std::optional<std::string> freeform_text) {
  ash::input_method::EditorMediator* editor_mediator =
      GetEditorMediator(profile_);
  if (editor_mediator != nullptr) {
    editor_mediator->HandleTrigger(std::move(preset_query_id),
                                   std::move(freeform_text));
  }
}

PickerClientImpl::PickerAppListControllerDelegate::
    PickerAppListControllerDelegate() = default;
PickerClientImpl::PickerAppListControllerDelegate::
    ~PickerAppListControllerDelegate() = default;

void PickerClientImpl::PickerAppListControllerDelegate::DismissView() {
  NOTIMPLEMENTED_LOG_ONCE();
}

aura::Window*
PickerClientImpl::PickerAppListControllerDelegate::GetAppListWindow() {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

int64_t
PickerClientImpl::PickerAppListControllerDelegate::GetAppListDisplayId() {
  NOTIMPLEMENTED_LOG_ONCE();
  return 0;
}

bool PickerClientImpl::PickerAppListControllerDelegate::IsAppPinned(
    const std::string& app_id) {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

bool PickerClientImpl::PickerAppListControllerDelegate::IsAppOpen(
    const std::string& app_id) const {
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
}

void PickerClientImpl::PickerAppListControllerDelegate::PinApp(
    const std::string& app_id) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void PickerClientImpl::PickerAppListControllerDelegate::UnpinApp(
    const std::string& app_id) {
  NOTIMPLEMENTED_LOG_ONCE();
}

AppListControllerDelegate::Pinnable
PickerClientImpl::PickerAppListControllerDelegate::GetPinnable(
    const std::string& app_id) {
  NOTIMPLEMENTED_LOG_ONCE();
  return AppListControllerDelegate::NO_PIN;
}

void PickerClientImpl::PickerAppListControllerDelegate::CreateNewWindow(
    bool incognito,
    bool should_trigger_session_restore) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void PickerClientImpl::PickerAppListControllerDelegate::OpenURL(
    Profile* profile,
    const GURL& url,
    ui::PageTransition transition,
    WindowOpenDisposition disposition) {
  NOTIMPLEMENTED_LOG_ONCE();
}
