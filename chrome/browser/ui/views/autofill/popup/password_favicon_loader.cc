// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/password_favicon_loader.h"

#include "base/functional/bind.h"
#include "chrome/browser/favicon/large_icon_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/favicon/core/large_icon_service.h"
#include "components/favicon_base/favicon_types.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/image/image.h"

namespace autofill {
namespace {

constexpr net::NetworkTrafficAnnotationTag kFaviconTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("autofill_password_favicon_fetch", R"(
          semantics {
            sender: "Favicon"
            description:
              "Sends a request to a Google server to retrieve a favicon bitmap "
              "for the domain associated with their credentials."
            trigger:
              "On the Autofill popup, favicons are loaded for manually "
              "triggered password suggestions."
            data: "Page URL and desired icon size."
            destination: GOOGLE_OWNED_SERVICE
            internal {
              contacts {
                owners: "//components/autofill/OWNERS"
              }
            }
            user_data {
              type: SENSITIVE_URL
            }
            last_reviewed: "2024-06-13"
          }
          policy {
            cookies_allowed: NO
            setting: "This feature cannot be disabled by settings."
            policy_exception_justification: "Not implemented."
          }
          comments: "No policy is necessary as the request cannot be turned "
            " off via settings."
          )");

}  // namespace

PasswordFaviconLoaderImpl::PasswordFaviconLoaderImpl(
    favicon::LargeIconService& favicon_service)
    : favicon_service_(favicon_service) {}
PasswordFaviconLoaderImpl::~PasswordFaviconLoaderImpl() = default;

void PasswordFaviconLoaderImpl::Load(
    const Suggestion::FaviconDetails& favicon_details,
    base::CancelableTaskTracker* task_tracker,
    OnLoadSuccess on_success,
    OnLoadFail on_fail) {
  if (!favicon_details.can_be_requested_from_google) {
    // TODO(crbug.com/325246516): Instead of fail, get favicon from website.
    std::move(on_fail).Run();
    return;
  }

  favicon_service_->GetLargeIconFromCacheFallbackToGoogleServer(
      favicon_details.domain_url,
      /*min_source_size=*/
      favicon::LargeIconService::StandardIconSize::k16x16,
      /*size_to_resize_to=*/
      favicon::LargeIconService::StandardIconSize::k16x16,
      favicon::LargeIconService::NoBigEnoughIconBehavior::kReturnEmpty,
      /*should_trim_page_url_path=*/false, kFaviconTrafficAnnotation,
      base::BindOnce(
          [](OnLoadSuccess on_success, OnLoadFail on_fail,
             const favicon_base::LargeIconResult& result) {
            if (result.bitmap.is_valid()) {
              std::move(on_success)
                  .Run(gfx::Image::CreateFrom1xPNGBytes(
                      result.bitmap.bitmap_data));
            } else {
              std::move(on_fail).Run();
            }
          },
          std::move(on_success), std::move(on_fail)),
      task_tracker);
}
}  // namespace autofill
