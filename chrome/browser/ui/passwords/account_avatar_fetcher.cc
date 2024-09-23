// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/account_avatar_fetcher.h"

#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

constexpr net::NetworkTrafficAnnotationTag kAccountAvatarTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("credenential_avatar", R"(
        semantics {
          sender: "Chrome Password Manager"
          description:
            "Every credential saved in Chromium via the Credential Management "
            "API can have an avatar URL. The URL is essentially provided by "
            "the site calling the API. The avatar is used in the account "
            "chooser UI and auto signin toast which appear when a site calls "
            "navigator.credentials.get(). The avatar is retrieved before "
            "showing the UI."
          trigger:
            "User visits a site that calls navigator.credentials.get(). "
            "Assuming there are matching credentials in the Chromium password "
            "store, the avatars are retrieved."
          data: "Only avatar URL, no user data."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "One can disable saving new credentials in the settings (see "
            "'Passwords and forms'). There is no setting to disable the API."
          chrome_policy {
            PasswordManagerEnabled {
                policy_options {mode: MANDATORY}
                PasswordManagerEnabled: false
            }
          }
        })");

}  // namespace

AccountAvatarFetcher::AccountAvatarFetcher(
    const GURL& url,
    const base::WeakPtr<AccountAvatarFetcherDelegate>& delegate)
    : fetcher_(url, this, kAccountAvatarTrafficAnnotation),
      delegate_(delegate) {}

AccountAvatarFetcher::~AccountAvatarFetcher() = default;

void AccountAvatarFetcher::Start(
    network::mojom::URLLoaderFactory* loader_factory,
    const url::Origin& initiator) {
  fetcher_.Init(net::ReferrerPolicy::NEVER_CLEAR,
                network::mojom::CredentialsMode::kOmit,
                net::HttpRequestHeaders(), initiator);
  fetcher_.Start(loader_factory);
}

void AccountAvatarFetcher::OnFetchComplete(const GURL& /*url*/,
                                           const SkBitmap* bitmap) {
  if (bitmap && delegate_) {
    delegate_->UpdateAvatar(gfx::ImageSkia::CreateFrom1xBitmap(*bitmap));
  }

  delete this;
}
