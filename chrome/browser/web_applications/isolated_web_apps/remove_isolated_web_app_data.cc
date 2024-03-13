// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/remove_isolated_web_app_data.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_reader_registry.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_reader_registry_factory.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "url/origin.h"

namespace web_app {

namespace {

class RemovalObserver : public content::BrowsingDataRemover::Observer {
 public:
  RemovalObserver(Profile* profile, base::OnceClosure callback)
      : profile_(profile),
        callback_(std::move(callback).Then(
            base::BindOnce(&base::DeletePointer<RemovalObserver>, this))) {
    profile_->GetBrowsingDataRemover()->AddObserver(this);
  }

  ~RemovalObserver() override {
    profile_->GetBrowsingDataRemover()->RemoveObserver(this);
  }

  void OnBrowsingDataRemoverDone(uint64_t failed_data_types) override {
    std::move(callback_).Run();
  }

 private:
  raw_ptr<Profile> profile_ = nullptr;
  base::OnceClosure callback_;  // Owns `this`.
};

void CloseBundle(Profile* profile,
                 const IwaSource& source,
                 base::OnceClosure callback) {
  absl::visit(
      base::Overloaded{
          [&](const IwaSourceBundle& bundle) {
            auto* reader_registry =
                IsolatedWebAppReaderRegistryFactory::GetForProfile(profile);
            if (!reader_registry) {
              std::move(callback).Run();
              return;
            }
            reader_registry->ClearCacheForPath(bundle.path(),
                                               std::move(callback));
          },
          [&](const IwaSourceProxy& proxy) { std::move(callback).Run(); },
      },
      source.variant());
}

}  // namespace

void RemoveIsolatedWebAppBrowsingData(Profile* profile,
                                      const url::Origin& iwa_origin,
                                      base::OnceClosure callback) {
  CHECK(iwa_origin.scheme() == chrome::kIsolatedAppScheme);

  auto filter = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::Mode::kDelete);
  filter->AddOrigin(iwa_origin);

  chrome_browsing_data_remover::DataType removal_mask =
      chrome_browsing_data_remover::DATA_TYPE_SITE_DATA;
  removal_mask |= content::BrowsingDataRemover::DATA_TYPE_CACHE;

  // BrowsingDataRemover doesn't support clearing cookies if origins are
  // present in the filter because cookies aren't origin-scoped. This is dealt
  // with in ChromeBrowsingDataRemoverDelegate::RemoveEmbedderData by making a
  // second BrowsingDataRemover::RemoveWithFilter call with a mask of only
  // DATA_TYPE_COOKIES and the filter's origins relaxed to domains. This won't
  // work for IWAs because they're not representable as domains.
  //
  // The desired IWA cookie deletion behavior is for all cookies in the IWA's
  // primary StoragePartition to be deleted, even those cross-origin to the
  // IWA. This can't be represented with BrowsingDataFilterBuilder and
  // BrowsingDataRemover::DATA_TYPE_COOKIES. Instead, we clear the
  // DATA_TYPE_COOKIES flag here and set DATA_TYPE_ISOLATED_WEB_APP_COOKIES,
  // and ChromeBrowsingDataRemoverDelegate will translate the
  // DATA_TYPE_ISOLATED_WEB_APP_COOKIES flag to DATA_TYPE_COOKIES when clearing
  // data in IWA-owned StoragePartitions.
  removal_mask &= ~content::BrowsingDataRemover::DATA_TYPE_COOKIES;
  removal_mask |=
      chrome_browsing_data_remover::DATA_TYPE_ISOLATED_WEB_APP_COOKIES;

  profile->GetBrowsingDataRemover()->RemoveWithFilterAndReply(
      /*delete_begin=*/base::Time(), /*delete_end=*/base::Time::Max(),
      removal_mask, chrome_browsing_data_remover::ALL_ORIGIN_TYPES,
      std::move(filter), new RemovalObserver(profile, std::move(callback)));
}

void CloseAndDeleteBundle(Profile* profile,
                          const IsolatedWebAppStorageLocation& location,
                          base::OnceClosure callback) {
  CloseBundle(
      profile,
      IwaSourceWithMode::FromStorageLocation(profile->GetPath(), location),
      base::BindOnce(CleanupLocationIfOwned, profile->GetPath(), location,
                     std::move(callback)));
}

}  // namespace web_app
