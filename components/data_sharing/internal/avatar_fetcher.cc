// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/avatar_fetcher.h"

#include "base/strings/stringprintf.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/signin/public/base/avatar_icon_util.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace {
constexpr char kUmaClientName[] = "DataSharingAvatarImageFetcher";

// static
constexpr net::NetworkTrafficAnnotationTag kAvatarFetcherTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("data_sharing_avatar_image_fetcher",
                                        R"(
        semantics {
          sender: "Image fetcher for data sharing people profile images"
          description:
            "Data sharing service needs to display the account avatars "
            "of users in a shared tab group on Chrome UI."
          trigger:
            "As required from Chrome UIs (tab group editor bubble "
            "and recent activity chips)."
          data: "No data sent in this request."
          destination: GOOGLE_OWNED_SERVICE
          user_data: {
            type: NONE
          }
          internal {
              contacts { email: "chrome-data-sharing-eng@google.com" }
            }
          last_reviewed: "2024-11-27"
        }
        policy {
          cookies_allowed: NO
          cookies_store: ""
          setting:
            "This feature cannot be disabled by settings as it is part of the "
            "Data Sharing."
          policy_exception_justification:
            "Not implemented."
        })");
}  // namespace

namespace data_sharing {
AvatarFetcher::AvatarFetcher() = default;
AvatarFetcher::~AvatarFetcher() = default;

void AvatarFetcher::Fetch(const GURL& avatar_url,
                          int size,
                          ImageCallback callback,
                          image_fetcher::ImageFetcher* image_fetcher) {
#if BUILDFLAG(IS_IOS)
  if (!image_fetcher) {
    return;
  }
#endif
  CHECK(image_fetcher);
  image_fetcher::ImageFetcherParams params(kAvatarFetcherTrafficAnnotation,
                                           kUmaClientName);

  // If server cannot return a user customized profile picture or monogram it
  // returns a default silhouette. This is not the style we want. We want a
  // filled person icon as the default. So no_silhouette is set to true to
  // enforce an empty image which will be handled by
  // AvatarFetcher::OnImageFetched.
  GURL image_url_with_size = signin::GetAvatarImageURLWithOptions(
      avatar_url, size, /*no_silhouette=*/true,
      signin::AvatarCropType::kCircle);

  image_fetcher->FetchImage(
      image_url_with_size,
      base::BindOnce(&AvatarFetcher::OnImageFetched,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      std::move(params));
}

void AvatarFetcher::OnImageFetched(
    ImageCallback callback,
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& metadata) {
  std::move(callback).Run(image);
}
}  // namespace data_sharing
