// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/web_applications/web_contents/web_app_data_retriever.h"
#include "chrome/browser/web_applications/web_contents/web_app_icon_downloader.h"
#include "chrome/browser/web_applications/web_contents/web_app_url_loader.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/url_constants.h"

namespace web_app {
namespace {
bool EqualsWithComparison(const GURL& a,
                          const GURL& b,
                          WebAppUrlLoader::UrlComparison url_comparison) {
  DCHECK(a.is_valid());
  DCHECK(b.is_valid());
  if (a == b) {
    return true;
  }
  GURL::Replacements replace;
  switch (url_comparison) {
    case WebAppUrlLoader::UrlComparison::kExact:
      return false;
    case WebAppUrlLoader::UrlComparison::kSameOrigin:
      replace.ClearPath();
      [[fallthrough]];
    case WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef:
      replace.ClearQuery();
      replace.ClearRef();
      break;
  }
  return a.ReplaceComponents(replace) == b.ReplaceComponents(replace);
}
}  // namespace

class FakeWebContentsManager::FakeUrlLoader : public WebAppUrlLoader {
 public:
  explicit FakeUrlLoader(base::WeakPtr<FakeWebContentsManager> manager)
      : manager_(manager) {}
  ~FakeUrlLoader() override = default;

  void PrepareForLoad(content::WebContents* web_contents,
                      ResultCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), WebAppUrlLoaderResult::kUrlLoaded));
    manager_->loaded_urls_[web_contents] = GURL(url::kAboutBlankURL);
  }

  void LoadUrl(const GURL& url,
               content::WebContents* web_contents,
               UrlComparison url_comparison,
               ResultCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(manager_);
    auto page_it = manager_->page_state_.find(url);
    if (page_it == manager_->page_state_.end()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback),
                         WebAppUrlLoaderResult::kFailedErrorPageLoaded));
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
            base::BindOnce(std::move(callback),
                           WebAppUrlLoaderResult::kRedirectedUrlLoaded));
        return;
      }
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), page.url_load_result));
  }

 private:
  base::WeakPtr<FakeWebContentsManager> manager_;

  SEQUENCE_CHECKER(sequence_checker_);
};

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
    CHECK(url.is_valid() || url.is_empty())
        << "No url has been loaded on this web contents. " << url.spec();
    auto page_it = manager_->page_state_.find(url);
    if (page_it == manager_->page_state_.end()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), nullptr));
      return;
    }
    FakeWebContentsManager::FakePageState& page = page_it->second;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  page.page_install_info
                                      ? std::make_unique<WebAppInstallInfo>(
                                            page.page_install_info->Clone())
                                      : std::unique_ptr<WebAppInstallInfo>()));
  }

  void CheckInstallabilityAndRetrieveManifest(
      content::WebContents* web_contents,
      bool bypass_service_worker_check,
      CheckInstallabilityCallback callback,
      absl::optional<webapps::InstallableParams> params) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(manager_);
    GURL url = manager_->loaded_urls_[web_contents];
    CHECK(url.is_valid() || url.is_empty())
        << "No url has been loaded on this web contents. " << url.spec();
    auto page_it = manager_->page_state_.find(url);
    if (page_it == manager_->page_state_.end()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback), blink::mojom::ManifestPtr(),
                         GURL(), /*valid_manifest_for_web_app=*/false,
                         webapps::InstallableStatusCode::NO_MANIFEST));
      return;
    }
    FakeWebContentsManager::FakePageState& page = page_it->second;

    if (page.on_manifest_fetch) {
      std::move(page.on_manifest_fetch).Run();
    }

    if (!bypass_service_worker_check && !page.has_service_worker) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              std::move(callback), page.opt_manifest.Clone(), page.manifest_url,
              false,
              webapps::InstallableStatusCode::NO_MATCHING_SERVICE_WORKER));
      return;
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), page.opt_manifest.Clone(),
                       page.manifest_url, page.valid_manifest_for_web_app,
                       page.error_code));
  }

  void GetIcons(content::WebContents* web_contents,
                base::flat_set<GURL> icon_urls,
                bool skip_page_favicons,
                GetIconsCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(manager_);
    IconsMap icons_map;
    DownloadedIconsHttpResults per_icon_results;
    for (const GURL& icon_url : icon_urls) {
      auto icons_it = manager_->icon_state_.find(icon_url);
      if (icons_it == manager_->icon_state_.end()) {
        per_icon_results[icon_url] = 404;
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
      per_icon_results[icon_url] = icon.http_status_code;
    }

    // Add favicon if requested & available.
    if (!skip_page_favicons) {
      GURL url = manager_->loaded_urls_[web_contents];
      CHECK(url.is_valid() || url.is_empty())
          << "No url has been loaded on this web contents. " << url.spec();
      auto page_it = manager_->page_state_.find(url);
      if (page_it != manager_->page_state_.end()) {
        FakeWebContentsManager::FakePageState& page = page_it->second;
        if (!page.favicon_url.is_empty()) {
          icons_map[page.favicon_url] = page.favicon;
          per_icon_results[page.favicon_url] = page.favicon.empty() ? 404 : 200;
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

FakeWebContentsManager::FakePageState::FakePageState() = default;
FakeWebContentsManager::FakePageState::~FakePageState() = default;
FakeWebContentsManager::FakePageState::FakePageState(FakePageState&&) = default;

FakeWebContentsManager::FakeIconState::FakeIconState() = default;
FakeWebContentsManager::FakeIconState::~FakeIconState() = default;

FakeWebContentsManager::FakeWebContentsManager() = default;
FakeWebContentsManager::~FakeWebContentsManager() = default;

void FakeWebContentsManager::SetUrlLoaded(content::WebContents* web_contents,
                                          const GURL& url) {
  loaded_urls_[web_contents] = url;
}

std::unique_ptr<WebAppUrlLoader> FakeWebContentsManager::CreateUrlLoader() {
  return std::make_unique<FakeUrlLoader>(weak_factory_.GetWeakPtr());
}

std::unique_ptr<WebAppDataRetriever>
FakeWebContentsManager::CreateDataRetriever() {
  return std::make_unique<FakeWebAppDataRetriever>(weak_factory_.GetWeakPtr());
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

void FakeWebContentsManager::SetPageState(
    const GURL& gurl,
    FakeWebContentsManager::FakePageState page_state) {
  page_state_.emplace(gurl, std::move(page_state));
}
FakeWebContentsManager::FakePageState&
FakeWebContentsManager::GetOrCreatePageState(const GURL& gurl) {
  return page_state_[gurl];
}
void FakeWebContentsManager::DeletePageState(const GURL& gurl) {
  page_state_.erase(gurl);
}

base::WeakPtr<FakeWebContentsManager> FakeWebContentsManager::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace web_app
