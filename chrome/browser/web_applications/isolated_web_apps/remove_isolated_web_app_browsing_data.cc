// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/remove_isolated_web_app_browsing_data.h"

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/profiles/profile.h"
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
  raw_ptr<Profile> profile_;
  base::OnceClosure callback_;  // Owns `this`.
};

}  // namespace

void RemoveIsolatedWebAppBrowsingData(Profile* profile,
                                      const url::Origin& iwa_origin,
                                      base::OnceClosure callback) {
  CHECK(iwa_origin.scheme() == chrome::kIsolatedAppScheme);

  auto filter = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::Mode::kDelete);
  filter->AddOrigin(iwa_origin);

  // TODO(crbug.com/1449362): BrowsingDataRemover doesn't support clearing
  // cookies if origins are present in the filter, so we remove
  // DATA_TYPE_COOKIES here even though we do want them cleared.
  profile->GetBrowsingDataRemover()->RemoveWithFilterAndReply(
      /*delete_begin=*/base::Time(), /*delete_end=*/base::Time::Max(),
      chrome_browsing_data_remover::DATA_TYPE_SITE_DATA &
          ~content::BrowsingDataRemover::DATA_TYPE_COOKIES,
      chrome_browsing_data_remover::ALL_ORIGIN_TYPES, std::move(filter),
      new RemovalObserver(profile, std::move(callback)));
}

}  // namespace web_app
