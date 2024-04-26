// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/service_worker_payment_app_finder.h"

#include <algorithm>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/stl_util.h"
#include "base/supports_user_data.h"
#include "base/task/single_thread_task_runner.h"
#include "components/payments/content/developer_console_logger.h"
#include "components/payments/content/installable_payment_app_crawler.h"
#include "components/payments/content/manifest_verifier.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/utility/payment_manifest_parser.h"
#include "components/payments/core/features.h"
#include "components/payments/core/method_strings.h"
#include "components/payments/core/payment_manifest_downloader.h"
#include "components/payments/core/url_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/payment_app_provider.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/stored_payment_app.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "ui/gfx/image/image.h"
#include "url/url_canon.h"

namespace payments {
namespace {

// Returns true if |app| supports at least one of the |requests|.
bool AppSupportsAtLeastOneRequestedMethodData(
    const content::StoredPaymentApp& app,
    const std::vector<mojom::PaymentMethodDataPtr>& requests) {
  for (const auto& enabled_method : app.enabled_methods) {
    for (const auto& request : requests) {
      if (enabled_method == request->supported_method) {
        return true;
      }
    }
  }
  return false;
}

void RemovePortNumbersFromScopesForTest(
    content::InstalledPaymentAppsFinder::PaymentApps* apps) {
  GURL::Replacements replacements;
  replacements.ClearPort();
  for (auto& app : *apps) {
    app.second->scope = app.second->scope.ReplaceComponents(replacements);
  }
}

class SelfDeletingServiceWorkerPaymentAppFinder
    : public base::SupportsUserData::Data {
 public:
  static base::WeakPtr<SelfDeletingServiceWorkerPaymentAppFinder>
  CreateAndSetOwnedBy(content::WebContents* owner) {
    auto owned =
        std::make_unique<SelfDeletingServiceWorkerPaymentAppFinder>(owner);
    auto* pointer = owned.get();
    owner->SetUserData(pointer, std::move(owned));
    return pointer->weak_ptr_factory_.GetWeakPtr();
  }

  explicit SelfDeletingServiceWorkerPaymentAppFinder(
      content::WebContents* owner)
      : owner_(owner) {}

  SelfDeletingServiceWorkerPaymentAppFinder(
      const SelfDeletingServiceWorkerPaymentAppFinder& other) = delete;
  SelfDeletingServiceWorkerPaymentAppFinder& operator=(
      const SelfDeletingServiceWorkerPaymentAppFinder& other) = delete;

  ~SelfDeletingServiceWorkerPaymentAppFinder() override = default;

  // After |callback| has fired, the factory refreshes its own cache in the
  // background. Once the cache has been refreshed, the factory invokes the
  // |finished_using_resources_callback|. At this point, it's safe to delete
  // this factory. Don't destroy the factory and don't call this method again
  // until |finished_using_resources_callback| has run.
  void GetAllPaymentApps(
      const url::Origin& merchant_origin,
      content::RenderFrameHost* initiator_render_frame_host,
      std::unique_ptr<PaymentManifestDownloader> downloader,
      scoped_refptr<PaymentManifestWebDataService> cache,
      const std::vector<mojom::PaymentMethodDataPtr>& requested_method_data,
      ServiceWorkerPaymentAppFinder::GetAllPaymentAppsCallback callback,
      base::OnceClosure finished_using_resources_callback) {
    DCHECK(!verifier_);
    DCHECK(initiator_render_frame_host);
    DCHECK(initiator_render_frame_host->IsActive());

    downloader_ = std::move(downloader);

    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(initiator_render_frame_host);
    parser_ = std::make_unique<PaymentManifestParser>(
        std::make_unique<DeveloperConsoleLogger>(web_contents));
    cache_ = cache;
    verifier_ = std::make_unique<ManifestVerifier>(
        merchant_origin, web_contents, downloader_.get(), parser_.get(),
        cache_.get());
    if (base::FeatureList::IsEnabled(
            features::kWebPaymentsJustInTimePaymentApp)) {
      crawler_ = std::make_unique<InstallablePaymentAppCrawler>(
          merchant_origin, initiator_render_frame_host, downloader_.get(),
          parser_.get(), cache_.get());
      if (ignore_port_in_origin_comparison_for_testing_)
        crawler_->IgnorePortInOriginComparisonForTesting();
    }

    // Method data cannot be copied and is passed in as a const-ref, which
    // cannot be moved, so make a manual copy for using below.
    for (const auto& method_data : requested_method_data) {
      requested_method_data_.emplace_back(method_data.Clone());
    }
    callback_ = std::move(callback);
    finished_using_resources_callback_ =
        std::move(finished_using_resources_callback);

    content::InstalledPaymentAppsFinder::GetInstance(
        initiator_render_frame_host->GetBrowserContext())
        ->GetAllPaymentApps(base::BindOnce(
            &SelfDeletingServiceWorkerPaymentAppFinder::OnGotAllPaymentApps,
            weak_ptr_factory_.GetWeakPtr()));
  }

  void IgnorePortInOriginComparisonForTesting() {
    ignore_port_in_origin_comparison_for_testing_ = true;
  }

 private:
  // base::SupportsUserData::Data implementation.
  std::unique_ptr<Data> Clone() override {
    return nullptr;  // Cloning is not supported.
  }

  static void RemoveUnrequestedMethods(
      const std::vector<mojom::PaymentMethodDataPtr>& requested_method_data,
      content::InstalledPaymentAppsFinder::PaymentApps* apps) {
    std::set<std::string> requested_methods;
    for (const auto& requested_method_datum : requested_method_data) {
      requested_methods.insert(requested_method_datum->supported_method);
    }
    for (auto& app : *apps) {
      std::sort(app.second->enabled_methods.begin(),
                app.second->enabled_methods.end());
      app.second->enabled_methods =
          base::STLSetIntersection<std::vector<std::string>>(
              app.second->enabled_methods, requested_methods);
    }
  }

  void OnGotAllPaymentApps(
      content::InstalledPaymentAppsFinder::PaymentApps apps) {
    if (ignore_port_in_origin_comparison_for_testing_)
      RemovePortNumbersFromScopesForTest(&apps);

    RemoveUnrequestedMethods(requested_method_data_, &apps);
    ServiceWorkerPaymentAppFinder::RemoveAppsWithoutMatchingMethodData(
        requested_method_data_, &apps);
    if (apps.empty()) {
      OnPaymentAppsVerified(std::move(apps), first_error_message_);
      OnPaymentAppsVerifierFinishedUsingResources();
      return;
    }

    // The |verifier_| will invoke |OnPaymentAppsVerified| with the list of all
    // valid payment apps. This list may be empty, if none of the apps were
    // found to be valid.
    is_payment_verifier_finished_using_resources_ = false;
    verifier_->Verify(
        std::move(apps),
        base::BindOnce(
            &SelfDeletingServiceWorkerPaymentAppFinder::OnPaymentAppsVerified,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&SelfDeletingServiceWorkerPaymentAppFinder::
                           OnPaymentAppsVerifierFinishedUsingResources,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnPaymentAppsVerified(
      content::InstalledPaymentAppsFinder::PaymentApps apps,
      const std::string& error_message) {
    if (first_error_message_.empty())
      first_error_message_ = error_message;

    installed_apps_ = std::move(apps);

    // TODO(crbug.com/40259220): Once kPaymentHandlerAlwaysRefreshIcon is rolled
    // out fully, remove the 'missing icons' path and rely on the refresh path
    // to handle any payment app that is missing an icon. This will cause fixing
    // missing icons to be async rather than synchronous, but by now this is a
    // very rare case anyway.
    std::set<GURL> method_manifest_urls_for_missing_icons;
    std::set<GURL> method_manifest_urls_for_icon_refresh;
    for (auto& app : installed_apps_) {
      for (const auto& method : app.second->enabled_methods) {
        // Only payment methods with manifests are eligible for refetching the
        // icon of their installed payment apps.
        GURL method_manifest_url = GURL(method);
        if (!UrlUtil::IsValidUrlBasedPaymentMethodIdentifier(
                method_manifest_url)) {
          continue;
        }
        method_manifest_urls_for_icon_refresh.insert(method_manifest_url);

        if (!app.second->icon.get() || app.second->icon.get()->drawsNothing()) {
          method_manifest_urls_for_missing_icons.insert(method_manifest_url);
        }
      }
    }

    // Crawl installable web payment apps if no web payment apps have been
    // installed or when an installed app is missing an icon.
    //
    // If `crawler_` is null, that indicates that JIT install has been disabled
    // entirely, and so we should be doing no crawling.
    if ((installed_apps_.empty() ||
         !method_manifest_urls_for_missing_icons.empty()) &&
        crawler_ != nullptr) {
      is_payment_app_crawler_finished_using_resources_ = false;
      crawler_->Start(
          requested_method_data_,
          std::move(method_manifest_urls_for_missing_icons),
          base::BindOnce(
              &SelfDeletingServiceWorkerPaymentAppFinder::OnPaymentAppsCrawled,
              weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(&SelfDeletingServiceWorkerPaymentAppFinder::
                             OnPaymentAppsCrawlerFinishedUsingResources,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    }

    // To ensure that payment apps are able to respond to web-app manifest
    // changes (such as their icon changing), we refetch the web-app manifest
    // for already-installed apps. This process can be slow, so we don't block
    // the creation of payment apps on it - the app will be updated in the
    // background and changes will take effect on any subsequent Payment Request
    // launch.
    if (base::FeatureList::IsEnabled(
            features::kPaymentHandlerAlwaysRefreshIcon) &&
        !method_manifest_urls_for_icon_refresh.empty() && crawler_ != nullptr) {
      DCHECK(!installed_apps_.empty());
      is_payment_app_crawler_finished_using_resources_ = false;
      crawler_->Start(
          requested_method_data_,
          std::move(method_manifest_urls_for_icon_refresh),
          base::BindOnce(&SelfDeletingServiceWorkerPaymentAppFinder::
                             OnPaymentAppsCrawledForUpdatedInfo,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(&SelfDeletingServiceWorkerPaymentAppFinder::
                             OnPaymentAppsCrawlerFinishedUsingResources,
                         weak_ptr_factory_.GetWeakPtr()));

      // Deliberately copy installed_apps_, as it is still needed in
      // |OnPaymentAppsCrawledForUpdatedInfo|.
      content::InstalledPaymentAppsFinder::PaymentApps installed_apps_copy;
      for (const auto& app : installed_apps_) {
        installed_apps_copy[app.first] =
            std::make_unique<content::StoredPaymentApp>(*app.second);
      }
      std::move(callback_).Run(
          std::move(installed_apps_copy),
          ServiceWorkerPaymentAppFinder::InstallablePaymentApps(),
          first_error_message_);
    } else {
      // Release crawler_ since it will not be used from now on.
      crawler_.reset();

      std::move(callback_).Run(
          std::move(installed_apps_),
          ServiceWorkerPaymentAppFinder::InstallablePaymentApps(),
          first_error_message_);
    }
  }

  void OnPaymentAppsCrawled(
      std::map<GURL, std::unique_ptr<WebAppInstallationInfo>> apps_info,
      std::map<GURL, std::unique_ptr<RefetchedIcon>> refetched_icons,
      const std::string& error_message) {
    if (first_error_message_.empty())
      first_error_message_ = error_message;

    UpdatePaymentAppIcons(refetched_icons);

    std::move(callback_).Run(std::move(installed_apps_), std::move(apps_info),
                             first_error_message_);
  }

  void OnPaymentAppsCrawledForUpdatedInfo(
      std::map<GURL, std::unique_ptr<WebAppInstallationInfo>> apps_info,
      std::map<GURL, std::unique_ptr<RefetchedIcon>> refetched_icons,
      // We deliberately ignore the error message, as this method is an optional
      // asynchronous update - if it failed, it is ok to fail silently.
      const std::string& ignored_error_message) {
    // This crawl should only have been triggered for refetched icons, and in
    // that mode the crawler should not suggest installable apps to us.
    DCHECK(apps_info.empty());

    // TODO(crbug.com/40259220): Consider optimizing either this database write
    // or the entire re-crawling process to avoid fetching/saving icons when
    // nothing has changed in the manifest.
    UpdatePaymentAppIcons(refetched_icons);
  }

  void UpdatePaymentAppIcons(
      const std::map<GURL, std::unique_ptr<RefetchedIcon>>& refetched_icons) {
    for (auto& refetched_icon : refetched_icons) {
      GURL web_app_manifest_url = refetched_icon.first;
      RefetchedIcon* data = refetched_icon.second.get();
      for (auto& app : installed_apps_) {
        // It is possible (unlikely) to have multiple apps with same origins.
        // The proper validation is to store web_app_manifest_url in
        // StoredPaymentApp and confirm that it is the same as the
        // web_app_manifest_url from which icon is fetched.
        if (crawler_->IsSameOriginWith(GURL(app.second->scope),
                                       web_app_manifest_url)) {
          UpdatePaymentAppIcon(app.second, data->icon, data->method_name);
          app.second->icon = std::move(data->icon);
          break;
        }
      }
    }
  }

  void UpdatePaymentAppIcon(
      const std::unique_ptr<content::StoredPaymentApp>& app,
      const std::unique_ptr<SkBitmap>& icon,
      const std::string& method_name) {
    number_of_app_icons_to_update_++;

    DCHECK(!icon->empty());
    gfx::Image decoded_image = gfx::Image::CreateFrom1xBitmap(*(icon));
    scoped_refptr<base::RefCountedMemory> raw_data =
        decoded_image.As1xPNGBytes();
    std::string string_encoded_icon = base::Base64Encode(*raw_data);

    content::PaymentAppProvider::GetOrCreateForWebContents(owner_)
        ->UpdatePaymentAppIcon(
            app->registration_id, app->scope.spec(), app->name,
            string_encoded_icon, method_name, app->supported_delegations,
            base::BindOnce(&SelfDeletingServiceWorkerPaymentAppFinder::
                               OnUpdatePaymentAppIcon,
                           weak_ptr_factory_.GetWeakPtr()));
  }

  void OnUpdatePaymentAppIcon(payments::mojom::PaymentHandlerStatus status) {
    DCHECK(number_of_app_icons_to_update_ > 0);
    number_of_app_icons_to_update_--;
    if (number_of_app_icons_to_update_ == 0)
      FinishUsingResourcesIfReady();
  }

  void OnPaymentAppsCrawlerFinishedUsingResources() {
    crawler_.reset();

    is_payment_app_crawler_finished_using_resources_ = true;
    FinishUsingResourcesIfReady();
  }

  void OnPaymentAppsVerifierFinishedUsingResources() {
    verifier_.reset();

    is_payment_verifier_finished_using_resources_ = true;
    FinishUsingResourcesIfReady();
  }

  void FinishUsingResourcesIfReady() {
    if (is_payment_verifier_finished_using_resources_ &&
        is_payment_app_crawler_finished_using_resources_ &&
        !finished_using_resources_callback_.is_null() &&
        number_of_app_icons_to_update_ == 0) {
      downloader_.reset();
      parser_.reset();
      std::move(finished_using_resources_callback_).Run();

      base::SingleThreadTaskRunner::GetCurrentDefault()->PostNonNestableTask(
          FROM_HERE,
          base::BindOnce(&SelfDeletingServiceWorkerPaymentAppFinder::DeleteSelf,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }

  void DeleteSelf() { owner_->RemoveUserData(this); }

  // |owner_| owns this SelfDeletingServiceWorkerPaymentAppFinder, so it is
  // always valid.
  raw_ptr<content::WebContents> owner_;

  std::unique_ptr<PaymentManifestDownloader> downloader_;
  std::unique_ptr<PaymentManifestParser> parser_;
  scoped_refptr<PaymentManifestWebDataService> cache_;
  std::vector<mojom::PaymentMethodDataPtr> requested_method_data_;
  ServiceWorkerPaymentAppFinder::GetAllPaymentAppsCallback callback_;
  base::OnceClosure finished_using_resources_callback_;
  std::string first_error_message_;

  std::unique_ptr<ManifestVerifier> verifier_;
  bool is_payment_verifier_finished_using_resources_ = true;

  std::unique_ptr<InstallablePaymentAppCrawler> crawler_;
  bool is_payment_app_crawler_finished_using_resources_ = true;

  bool ignore_port_in_origin_comparison_for_testing_ = false;

  content::InstalledPaymentAppsFinder::PaymentApps installed_apps_;

  size_t number_of_app_icons_to_update_ = 0;

  base::WeakPtrFactory<SelfDeletingServiceWorkerPaymentAppFinder>
      weak_ptr_factory_{this};
};

}  // namespace

ServiceWorkerPaymentAppFinder::~ServiceWorkerPaymentAppFinder() = default;

void ServiceWorkerPaymentAppFinder::GetAllPaymentApps(
    const url::Origin& merchant_origin,
    scoped_refptr<PaymentManifestWebDataService> cache,
    std::vector<mojom::PaymentMethodDataPtr> requested_method_data,
    base::WeakPtr<CSPChecker> csp_checker,
    GetAllPaymentAppsCallback callback,
    base::OnceClosure finished_writing_cache_callback_for_testing) {
  DCHECK(!requested_method_data.empty());

  if (!render_frame_host().IsActive())
    return;

  // Do not look up payment handlers for ignored payment methods.
  std::erase_if(requested_method_data,
                [&](const mojom::PaymentMethodDataPtr& method_data) {
                  return base::Contains(ignored_methods_,
                                        method_data->supported_method);
                });
  if (requested_method_data.empty()) {
    std::move(callback).Run(
        content::InstalledPaymentAppsFinder::PaymentApps(),
        std::map<GURL, std::unique_ptr<WebAppInstallationInfo>>(),
        /*error_message=*/"");
    return;
  }

  auto* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host());
  auto self_delete_factory =
      SelfDeletingServiceWorkerPaymentAppFinder::CreateAndSetOwnedBy(
          web_contents);

  std::unique_ptr<PaymentManifestDownloader> downloader;
  if (test_downloader_ != nullptr) {
    test_downloader_->SetCSPCheckerForTesting(csp_checker);  // IN-TEST
    downloader = std::move(test_downloader_);
    self_delete_factory->IgnorePortInOriginComparisonForTesting();
  } else {
    downloader = std::make_unique<payments::PaymentManifestDownloader>(
        std::make_unique<DeveloperConsoleLogger>(web_contents), csp_checker,
        render_frame_host()
            .GetBrowserContext()
            ->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess());
  }

  self_delete_factory->GetAllPaymentApps(
      merchant_origin, &render_frame_host(), std::move(downloader), cache,
      requested_method_data, std::move(callback),
      std::move(finished_writing_cache_callback_for_testing));
}

// static
void ServiceWorkerPaymentAppFinder::RemoveAppsWithoutMatchingMethodData(
    const std::vector<mojom::PaymentMethodDataPtr>& requested_method_data,
    content::InstalledPaymentAppsFinder::PaymentApps* apps) {
  for (auto it = apps->begin(); it != apps->end();) {
    if (AppSupportsAtLeastOneRequestedMethodData(*it->second,
                                                 requested_method_data)) {
      ++it;
    } else {
      it = apps->erase(it);
    }
  }
}

void ServiceWorkerPaymentAppFinder::IgnorePaymentMethodForTest(
    const std::string& method) {
  ignored_methods_.insert(method);
}

ServiceWorkerPaymentAppFinder::ServiceWorkerPaymentAppFinder(
    content::RenderFrameHost* rfh)
    : content::DocumentUserData<ServiceWorkerPaymentAppFinder>(rfh),
      ignored_methods_({methods::kGooglePlayBilling}),
      test_downloader_(nullptr) {}

void ServiceWorkerPaymentAppFinder::
    SetDownloaderAndIgnorePortInOriginComparisonForTesting(
        std::unique_ptr<PaymentManifestDownloader> downloader) {
  test_downloader_ = std::move(downloader);
}

DOCUMENT_USER_DATA_KEY_IMPL(ServiceWorkerPaymentAppFinder);

}  // namespace payments
