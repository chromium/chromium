// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_app_icon_downloader.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/url_constants.h"

namespace web_app {
namespace {
bool EqualsWithComparison(
    const GURL& a,
    const GURL& b,
    webapps::WebAppUrlLoader::UrlComparison url_comparison) {
  DCHECK(a.is_valid());
  DCHECK(b.is_valid());
  if (a == b) {
    return true;
  }
  GURL::Replacements replace;
  switch (url_comparison) {
    case webapps::WebAppUrlLoader::UrlComparison::kExact:
      return false;
    case webapps::WebAppUrlLoader::UrlComparison::kSameOrigin:
      replace.ClearPath();
      [[fallthrough]];
    case webapps::WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef:
      replace.ClearQuery();
      replace.ClearRef();
      break;
  }
  return a.ReplaceComponents(replace) == b.ReplaceComponents(replace);
}
}  // namespace

// TODO(http://b/262606416): Replace FakeWebAppUrlLoader with this by redoing
// how the web contents dependency is wrapped.
class FakeWebContentsManager::FakeUrlLoader : public webapps::WebAppUrlLoader {
 public:
  explicit FakeUrlLoader(base::WeakPtr<FakeWebContentsManager> manager)
      : manager_(manager) {}
  ~FakeUrlLoader() override = default;

  void LoadUrl(content::NavigationController::LoadURLParams load_url_params,
               content::WebContents* web_contents,
               UrlComparison url_comparison,
               ResultCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    const GURL& url = load_url_params.url;
    CHECK(manager_);
    DVLOG(1) << "FakeWebContentsManager::FakeUrlLoader::LoadUrl " << url.spec();

    manager_->load_url_tracker_.Run(std::ref(load_url_params), web_contents,
                                    url_comparison);

    auto page_it = manager_->page_state_.find(url);
    if (page_it == manager_->page_state_.end()) {
      DLOG(WARNING) << "No page state at url: " << url.spec();
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              std::move(callback),
              webapps::WebAppUrlLoaderResult::kFailedErrorPageLoaded));
      return;
    }
    FakeWebContentsManager::FakePageState& page = page_it->second;
    manager_->loaded_urls_[web_contents] = url;
    if (page.redirection_url) {
      manager_->loaded_urls_[web_contents] = page.redirection_url.value();
      if (!EqualsWithComparison(url, page.redirection_url.value(),
                                url_comparison)) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(
                std::move(callback),
                webapps::WebAppUrlLoaderResult::kRedirectedUrlLoaded));
        return;
      }
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), page.url_load_result));
  }

  void PrepareForLoad(content::WebContents* web_contents,
                      base::OnceClosure callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callback));
  }

 private:
  base::WeakPtr<FakeWebContentsManager> manager_;

  SEQUENCE_CHECKER(sequence_checker_);
};

class FakeWebContentsManager::FakeWebAppIconDownloader
    : public WebAppIconDownloader {
 public:
  explicit FakeWebAppIconDownloader(
      base::WeakPtr<FakeWebContentsManager> web_contents_manager)
      : manager_(std::move(web_contents_manager)) {}
  ~FakeWebAppIconDownloader() override = default;

  void Start(content::WebContents* web_contents,
             const IconUrlSizeSet& extra_icon_urls,
             WebAppIconDownloaderCallback callback,
             IconDownloaderOptions options) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(manager_);
    IconsMap icons_map;
    DownloadedIconsHttpResults per_icon_results;
    for (const IconUrlWithSize& icon_url_with_size : extra_icon_urls) {
      const GURL& icon_url = icon_url_with_size.url;
      auto icons_it = manager_->icon_state_.find(icon_url);
      if (icons_it == manager_->icon_state_.end()) {
        DLOG(WARNING) << "No icon state at url: " << icon_url.spec();

        if (options.fail_all_if_any_fail) {
          // TODO: Test this codepath when migrating the
          // ManifestUpdateCheckCommand to use WebContentsManager.
          base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(callback),
                             IconsDownloadedResult::kAbortedDueToFailure,
                             IconsMap{}, std::move(per_icon_results)));
          return;
        }

        per_icon_results[icon_url_with_size] = 404;
        continue;
      }
      FakeWebContentsManager::FakeIconState& icon = icons_it->second;
      // The real implementation includes these CHECK statements, so this does
      // too.
      CHECK_LE(100, icon.http_status_code);
      CHECK_GT(600, icon.http_status_code);
      if (icon.trigger_primary_page_changed_if_fetched) {
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback),
                           IconsDownloadedResult::kPrimaryPageChanged,
                           IconsMap{}, DownloadedIconsHttpResults{}));
        return;
      }
      icons_map[icon_url] = icon.bitmaps;
      per_icon_results[icon_url_with_size] = icon.http_status_code;
      if (icon.bitmaps.empty() && options.fail_all_if_any_fail) {
        // TODO: Test this codepath when migrating the
        // ManifestUpdateCheckCommand to use WebContentsManager.
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(callback),
                           IconsDownloadedResult::kAbortedDueToFailure,
                           IconsMap{}, std::move(per_icon_results)));
        return;
      }
    }

    // Add favicon if requested & available.
    if (!options.skip_page_favicons) {
      GURL url = manager_->loaded_urls_[web_contents];
      CHECK(url.is_valid() || url.is_empty())
          << "No url has been loaded on this web contents. " << url.spec();
      auto page_it = manager_->page_state_.find(url);
      if (page_it != manager_->page_state_.end()) {
        FakeWebContentsManager::FakePageState& page = page_it->second;
        if (!page.favicon_url.is_empty()) {
          icons_map[page.favicon_url] = page.favicon;
          per_icon_results[IconUrlWithSize::CreateForUnspecifiedSize(
              page.favicon_url)] = page.favicon.empty() ? 404 : 200;
          if (page.favicon.empty() && options.fail_all_if_any_fail) {
            base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(callback),
                               IconsDownloadedResult::kAbortedDueToFailure,
                               IconsMap{}, std::move(per_icon_results)));
            return;
          }
        }
      }
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), IconsDownloadedResult::kCompleted,
                       std::move(icons_map), std::move(per_icon_results)));
  }

 private:
  base::WeakPtr<FakeWebContentsManager> manager_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// TODO(http://b/262606416): Replace FakeDataRetriever with this by redoing
// how the web contents dependency is wrapped.
class FakeWebContentsManager::FakeWebAppDataRetriever
    : public WebAppDataRetriever {
 public:
  explicit FakeWebAppDataRetriever(
      base::WeakPtr<FakeWebContentsManager> manager)
      : manager_(manager) {}
  ~FakeWebAppDataRetriever() override = default;

  void GetWebAppInstallInfo(content::WebContents* web_contents,
                            GetWebAppInstallInfoCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(manager_);
    GURL url = manager_->loaded_urls_[web_contents];
    DVLOG(1) << "FakeWebContentsManager::FakeWebAppDataRetriever::"
                "GetWebAppInstallInfo "
             << url.spec();
    CHECK(url.is_valid() || url.is_empty())
        << "No url has been loaded on this web contents. " << url.spec();
    auto page_it = manager_->page_state_.find(url);
    if (page_it == manager_->page_state_.end()) {
      DLOG(WARNING) << "No page state at url: " << url.spec();
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), nullptr));
      return;
    }
    FakeWebContentsManager::FakePageState& page = page_it->second;
    if (page.return_null_info) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), nullptr));
      return;
    }
    std::unique_ptr<WebAppInstallInfo> install_info =
        std::make_unique<WebAppInstallInfo>(
            GenerateManifestIdFromStartUrlOnly(url), url);
    install_info->title = page.title.value_or(base::UTF8ToUTF16(url.spec()));
    if (page.opt_metadata) {
      WebAppDataRetriever::PopulateWebAppInfoFromMetadata(install_info.get(),
                                                          *page.opt_metadata);
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(install_info)));
  }

  void CheckInstallabilityAndRetrieveManifest(
      content::WebContents* web_contents,
      CheckInstallabilityCallback callback,
      std::optional<webapps::InstallableParams> params) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(manager_);
    GURL url = manager_->loaded_urls_[web_contents];
    DVLOG(1) << "FakeWebContentsManager::FakeWebAppDataRetriever::"
                "CheckInstallabilityAndRetrieveManifest "
             << url.spec();
    CHECK(url.is_valid() || url.is_empty())
        << "No url has been loaded on this web contents. " << url.spec();
    auto page_it = manager_->page_state_.find(url);
    if (page_it == manager_->page_state_.end()) {
      DLOG(WARNING) << "No page state at url: " << url.spec();
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), blink::mojom::ManifestPtr(),
                         /*valid_manifest_for_web_app=*/false,
                         webapps::InstallableStatusCode::NO_MANIFEST));
      return;
    }
    FakeWebContentsManager::FakePageState& page = page_it->second;

    if (page.on_manifest_fetch) {
      std::move(page.on_manifest_fetch).Run();
    }

    // Apply the 'default' values in the manifest spec algorithm.
    blink::mojom::ManifestPtr manifest =
        page.manifest_before_default_processing
            ? page.manifest_before_default_processing->Clone()
            : blink::mojom::Manifest::New();
    manifest->manifest_url = page.manifest_url;
    if (manifest->start_url.is_empty()) {
      manifest->start_url = url;
    }
    if (manifest->id.is_empty()) {
      manifest->id = manifest->start_url.GetWithoutRef();
    }
    if (manifest->scope.is_empty()) {
      manifest->scope = manifest->start_url.GetWithoutFilename();
    }
    CHECK(manifest->scope.ExtractFileName().empty());

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::move(manifest),
                       page.valid_manifest_for_web_app, page.error_code));
  }

  void GetIcons(content::WebContents* web_contents,
                const IconUrlSizeSet& extra_favicon_urls,
                bool skip_page_favicons,
                bool fail_all_if_any_fail,
                GetIconsCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(manager_);
    std::unique_ptr<FakeWebAppIconDownloader> fake_downloader =
        std::make_unique<FakeWebAppIconDownloader>(manager_);
    FakeWebAppIconDownloader* downloader_ptr = fake_downloader.get();
    base::OnceClosure owning_callback =
        base::DoNothingWithBoundArgs(std::move(fake_downloader));
    downloader_ptr->Start(web_contents, extra_favicon_urls,
                          std::move(callback).Then(std::move(owning_callback)),
                          {.skip_page_favicons = skip_page_favicons,
                           .fail_all_if_any_fail = fail_all_if_any_fail});
  }

 private:
  base::WeakPtr<FakeWebContentsManager> manager_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// static
webapps::mojom::WebPageMetadataPtr
FakeWebContentsManager::CreateMetadataWithTitle(std::u16string title) {
  webapps::mojom::WebPageMetadataPtr metadata =
      webapps::mojom::WebPageMetadata::New();
  metadata->application_name = title;
  return metadata;
}

// static
webapps::mojom::WebPageMetadataPtr
FakeWebContentsManager::CreateMetadataWithIconAndTitle(std::u16string title,
                                                       GURL document_icon_url,
                                                       int32_t icon_size) {
  webapps::mojom::WebPageMetadataPtr metadata =
      webapps::mojom::WebPageMetadata::New();
  metadata->icons.push_back(
      webapps::mojom::WebPageIconInfo::New(document_icon_url, icon_size));
  metadata->application_name = title;
  return metadata;
}

FakeWebContentsManager::FakePageState::FakePageState() = default;
FakeWebContentsManager::FakePageState::~FakePageState() = default;
FakeWebContentsManager::FakePageState::FakePageState(FakePageState&&) = default;
FakeWebContentsManager::FakePageState&
FakeWebContentsManager::FakePageState::operator=(FakePageState&&) = default;

FakeWebContentsManager::FakeIconState::FakeIconState() = default;
FakeWebContentsManager::FakeIconState::~FakeIconState() = default;

FakeWebContentsManager::FakeWebContentsManager() = default;
FakeWebContentsManager::~FakeWebContentsManager() = default;

void FakeWebContentsManager::SetUrlLoaded(content::WebContents* web_contents,
                                          const GURL& url) {
  loaded_urls_[web_contents] = url;
}

std::unique_ptr<webapps::WebAppUrlLoader>
FakeWebContentsManager::CreateUrlLoader() {
  return std::make_unique<FakeUrlLoader>(weak_factory_.GetWeakPtr());
}

std::unique_ptr<WebAppDataRetriever>
FakeWebContentsManager::CreateDataRetriever() {
  return std::make_unique<FakeWebAppDataRetriever>(weak_factory_.GetWeakPtr());
}

std::unique_ptr<WebAppIconDownloader>
FakeWebContentsManager::CreateIconDownloader() {
  return std::make_unique<FakeWebAppIconDownloader>(weak_factory_.GetWeakPtr());
}

void FakeWebContentsManager::SetIconState(
    const GURL& icon_url,
    const FakeWebContentsManager::FakeIconState& icon_state) {
  icon_state_[icon_url] = icon_state;
}
FakeWebContentsManager::FakeIconState&
FakeWebContentsManager::GetOrCreateIconState(const GURL& icon_url) {
  return icon_state_[icon_url];
}
void FakeWebContentsManager::DeleteIconState(const GURL& icon_url) {
  icon_state_.erase(icon_url);
}

webapps::AppId FakeWebContentsManager::CreateBasicInstallPageState(
    const GURL& install_url,
    const GURL& manifest_url,
    const GURL& start_url,
    std::u16string_view name) {
  FakePageState& install_page_state = GetOrCreatePageState(install_url);
  install_page_state.url_load_result =
      webapps::WebAppUrlLoaderResult::kUrlLoaded;
  install_page_state.redirection_url = std::nullopt;

  install_page_state.title = u"Page title";

  install_page_state.manifest_url = manifest_url;
  install_page_state.valid_manifest_for_web_app = true;

  install_page_state.manifest_before_default_processing =
      blink::mojom::Manifest::New();
  install_page_state.manifest_before_default_processing->id =
      start_url.GetWithoutRef();
  install_page_state.manifest_before_default_processing->start_url = start_url;
  install_page_state.manifest_before_default_processing->display =
      blink::mojom::DisplayMode::kStandalone;
  install_page_state.manifest_before_default_processing->short_name = name;

  return GenerateAppId(/*manifest_id_path=*/std::nullopt, start_url);
}

void FakeWebContentsManager::SetPageState(
    const GURL& gurl,
    FakeWebContentsManager::FakePageState page_state) {
  page_state_[gurl] = std::move(page_state);
}
FakeWebContentsManager::FakePageState&
FakeWebContentsManager::GetOrCreatePageState(const GURL& gurl) {
  return page_state_[gurl];
}
void FakeWebContentsManager::DeletePageState(const GURL& gurl) {
  page_state_.erase(gurl);
}

bool FakeWebContentsManager::HasPageState(const GURL& gurl) {
  return page_state_.find(gurl) != page_state_.end();
}

void FakeWebContentsManager::TrackLoadUrlCalls(
    LoadUrlTracker load_url_tracker) {
  load_url_tracker_ = std::move(load_url_tracker);
}

base::WeakPtr<FakeWebContentsManager> FakeWebContentsManager::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace web_app
