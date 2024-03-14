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

#include "ash/picker/picker_controller.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/files/file_enumerator.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
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
#include "chrome/browser/ash/app_list/search/search_engine.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/ash_web_view_impl.h"
#include "chrome/browser/ui/webui/ash/emoji/emoji_picker.mojom-forward.h"
#include "chrome/browser/ui/webui/ash/emoji/emoji_picker.mojom-shared.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace ash {
enum class AppListSearchResultType;
}

namespace {

constexpr int kMaxGifsToSearch = 4;
constexpr base::span<std::string_view> kImageExtensions = {
    (std::string_view[]){".jpg", ".jpeg", ".png", ".gif", ".webp"}};

int GetAutocompleteProviderTypes(ash::PickerCategory category) {
  switch (category) {
    case ash::PickerCategory::kEmojis:
    case ash::PickerCategory::kSymbols:
    case ash::PickerCategory::kEmoticons:
    case ash::PickerCategory::kGifs:
    case ash::PickerCategory::kLocalFiles:
    case ash::PickerCategory::kDriveFiles:
      DLOG(FATAL) << "Unexpected category for autocomplete: "
                  << static_cast<int>(category);
      return 0;
    case ash::PickerCategory::kBookmarks:
      return AutocompleteProvider::TYPE_BOOKMARK;
    case ash::PickerCategory::kBrowsingHistory:
      return AutocompleteProvider::TYPE_HISTORY_QUICK |
             AutocompleteProvider::TYPE_HISTORY_URL |
             AutocompleteProvider::TYPE_HISTORY_FUZZY;
    case ash::PickerCategory::kOpenTabs:
      return AutocompleteProvider::TYPE_OPEN_TAB;
  }
}

int GetAllAutocompleteProviderTypes() {
  return GetAutocompleteProviderTypes(ash::PickerCategory::kBookmarks) |
         GetAutocompleteProviderTypes(ash::PickerCategory::kBrowsingHistory) |
         GetAutocompleteProviderTypes(ash::PickerCategory::kOpenTabs);
}

std::unique_ptr<app_list::SearchProvider> CreateDriveSearchProvider(
    Profile* profile) {
  return std::make_unique<app_list::DriveSearchProvider>(profile);
}

std::unique_ptr<app_list::SearchProvider> CreateFileSearchProvider(
    Profile* profile) {
  return std::make_unique<app_list::FileSearchProvider>(
      profile, base::FileEnumerator::FileType::FILES);
}

}  // namespace

PickerClientImpl::PickerClientImpl(ash::PickerController* controller,
                                   user_manager::UserManager* user_manager)
    : controller_(controller) {
  controller_->SetClient(this);

  // As `PickerClientImpl` is initialised in
  // `ChromeBrowserMainExtraPartsAsh::PostProfileInit`, the user manager does
  // not notify us of the first user "change".
  ActiveUserChanged(user_manager->GetActiveUser());
  user_session_state_observation_.Observe(user_manager);
}

PickerClientImpl::~PickerClientImpl() {
  controller_->SetClient(nullptr);
}

std::unique_ptr<ash::AshWebView> PickerClientImpl::CreateWebView(
    const ash::AshWebView::InitParams& params) {
  return std::make_unique<AshWebViewImpl>(params);
}

scoped_refptr<network::SharedURLLoaderFactory>
PickerClientImpl::GetSharedURLLoaderFactory() {
  CHECK(profile_);
  return profile_->GetURLLoaderFactory();
}

void PickerClientImpl::FetchGifSearch(const std::string& query,
                                      FetchGifsCallback callback) {
  CHECK(profile_);
  content::StoragePartition* storage_partition =
      profile_->GetDefaultStoragePartition();
  CHECK(storage_partition);
  // This will cancel the previous in-flight request if there is one.
  current_gif_fetcher_ = gif_tenor_api_fetcher_.FetchGifSearchCancellable(
      base::BindOnce(&PickerClientImpl::OnGifSearchResponse,
                     weak_factory_.GetWeakPtr(), std::move(callback), query),
      storage_partition->GetURLLoaderFactoryForBrowserProcess(), query,
      std::nullopt, kMaxGifsToSearch);
  current_gif_search_query_ = query;
}

void PickerClientImpl::OnGifSearchResponse(
    PickerClientImpl::FetchGifsCallback callback,
    std::string gif_search_query,
    emoji_picker::mojom::Status status,
    emoji_picker::mojom::TenorGifResponsePtr response) {
  if (gif_search_query != current_gif_search_query_) {
    // Do not call the callback at all if this is an old request.
    return;
  }
  if (status != emoji_picker::mojom::Status::kHttpOk) {
    // TODO: b/325368650 - Add better handling of errors.
    std::move(callback).Run({});
    return;
  }

  std::vector<ash::PickerSearchResult> picker_results;
  CHECK(response);
  picker_results.reserve(response->results.size());
  for (const emoji_picker::mojom::GifResponsePtr& result : response->results) {
    CHECK(result);
    const emoji_picker::mojom::GifUrlsPtr& urls = result->url;
    CHECK(urls);
    picker_results.push_back(ash::PickerSearchResult::Gif(
        urls->preview, urls->preview_image, result->preview_size, urls->full,
        result->full_size, base::UTF8ToUTF16(result->content_description)));
  }

  std::move(callback).Run(std::move(picker_results));
}

void PickerClientImpl::StopGifSearch() {
  current_gif_fetcher_.reset();
  current_gif_search_query_.reset();
}

void PickerClientImpl::StartCrosSearch(
    const std::u16string& query,
    std::optional<ash::PickerCategory> category,
    CrosSearchResultsCallback callback) {
  if (!category.has_value()) {
    CHECK(search_engine_);
    search_engine_->StartSearch(
        query, app_list::SearchOptions(),
        base::BindRepeating(&PickerClientImpl::OnCrosSearchResultsUpdated,
                            weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }

  switch (*category) {
    case ash::PickerCategory::kEmojis:
    case ash::PickerCategory::kSymbols:
    case ash::PickerCategory::kEmoticons:
    case ash::PickerCategory::kGifs:
      DLOG(FATAL) << "Unexpected category for StartCrosSearch: "
                  << static_cast<int>(*category);
      break;
    case ash::PickerCategory::kBookmarks:
    case ash::PickerCategory::kBrowsingHistory:
    case ash::PickerCategory::kOpenTabs:
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
      case ash::AppListSearchResultType::kOmnibox: {
        if (std::optional<GURL> result_url = result->url();
            result_url.has_value()) {
          picker_results.push_back(ash::PickerSearchResult::BrowsingHistory(
              *result_url, result->title(), result->icon().icon));
        } else {
          picker_results.push_back(
              ash::PickerSearchResult::Text(result->title()));
        }
        break;
      }
      case ash::AppListSearchResultType::kFileSearch: {
        // TODO: b/322926411 - Move this filtering to the search provider.
        bool is_image = false;
        for (std::string_view extension : kImageExtensions) {
          if (result->filePath().MatchesFinalExtension(extension)) {
            is_image = true;
            break;
          }
        }

        if (is_image) {
          picker_results.push_back(ash::PickerSearchResult::LocalFile(
              result->title(), result->filePath()));
        }
        break;
      }
      case ash::AppListSearchResultType::kDriveSearch:
        picker_results.push_back(ash::PickerSearchResult::DriveFile(
            result->title(), *result->url()));
        break;
      default:
        LOG(DFATAL) << "Got unexpected search result type "
                    << static_cast<int>(result->result_type());
        break;
    }
  }

  callback.Run(result_type, std::move(picker_results));
}

void PickerClientImpl::StopCrosQuery() {
  CHECK(search_engine_);
  search_engine_->StopQuery();
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
  search_engine_->AddProvider(
      CreateOmniboxProvider(GetAllAutocompleteProviderTypes()));
  search_engine_->AddProvider(CreateFileSearchProvider(profile_));
  search_engine_->AddProvider(CreateDriveSearchProvider(profile_));
}

std::unique_ptr<app_list::SearchProvider>
PickerClientImpl::CreateOmniboxProvider(int provider_types) {
  if (crosapi::browser_util::IsLacrosEnabled()) {
    // TODO: b/326147929 - Add autocomplete provider types for the Lacros
    // provider.
    return std::make_unique<app_list::OmniboxLacrosProvider>(
        profile_, &app_list_controller_delegate_,
        crosapi::CrosapiManager::Get());
  } else {
    return std::make_unique<app_list::OmniboxProvider>(
        profile_, &app_list_controller_delegate_, provider_types);
  }
}

std::unique_ptr<app_list::SearchProvider>
PickerClientImpl::CreateSearchProviderForCategory(
    ash::PickerCategory category) {
  switch (category) {
    case ash::PickerCategory::kEmojis:
    case ash::PickerCategory::kSymbols:
    case ash::PickerCategory::kEmoticons:
    case ash::PickerCategory::kGifs:
      DLOG(FATAL) << "Unexpected category for autocomplete: "
                  << static_cast<int>(category);
      return nullptr;
    case ash::PickerCategory::kBookmarks:
    case ash::PickerCategory::kBrowsingHistory:
    case ash::PickerCategory::kOpenTabs:
      return CreateOmniboxProvider(GetAutocompleteProviderTypes(category));
    case ash::PickerCategory::kLocalFiles:
      return CreateFileSearchProvider(profile_);
    case ash::PickerCategory::kDriveFiles:
      return CreateDriveSearchProvider(profile_);
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
