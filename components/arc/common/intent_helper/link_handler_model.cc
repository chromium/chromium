// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/common/intent_helper/link_handler_model.h"

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "components/arc/common/intent_helper/arc_intent_helper_package.h"
#include "components/google/core/common/google_util.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/components/arc/metrics/arc_metrics_service.h"
#endif

namespace arc {

namespace {

constexpr int kMaxValueLen = 2048;

bool GetQueryValue(const GURL& url,
                   std::string_view key_to_find,
                   std::u16string* out) {
  const std::string_view str = url.query_piece();

  url::Component query(0, str.length());
  url::Component key;
  url::Component value;

  while (url::ExtractQueryKeyValue(str, &query, &key, &value)) {
    if (value.is_empty()) {
      continue;
    }
    if (str.substr(key.begin, key.len) == key_to_find) {
      if (value.len >= kMaxValueLen) {
        return false;
      }
      url::RawCanonOutputW<kMaxValueLen> output;
      url::DecodeURLEscapeSequences(str.substr(value.begin, value.len),
                                    url::DecodeURLMode::kUTF8OrIsomorphic,
                                    &output);
      *out = std::u16string(output.view());
      return true;
    }
  }
  return false;
}

}  // namespace

// static
std::unique_ptr<LinkHandlerModel> LinkHandlerModel::Create(
    content::BrowserContext* context,
    const GURL& link_url,
    std::unique_ptr<ArcIntentHelperMojoDelegate> mojo_delegate) {
  CHECK(mojo_delegate);
  auto impl = base::WrapUnique(new LinkHandlerModel(std::move(mojo_delegate)));
  if (!impl->Init(context, link_url)) {
    return nullptr;
  }
  return impl;
}

LinkHandlerModel::~LinkHandlerModel() = default;

void LinkHandlerModel::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void LinkHandlerModel::OpenLinkWithHandler(uint32_t handler_id) {
  if (handler_id >= handlers_.size()) {
    return;
  }

  if (!mojo_delegate_->HandleUrl(url_.spec(),
                                 handlers_[handler_id].package_name)) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/40808069): Take metrics in Lacros as well.
  ArcMetricsService::RecordArcUserInteraction(
      context_, arc::UserInteractionType::APP_STARTED_FROM_LINK_CONTEXT_MENU);
#endif
}

LinkHandlerModel::LinkHandlerModel(
    std::unique_ptr<ArcIntentHelperMojoDelegate> mojo_delegate)
    : mojo_delegate_(std::move(mojo_delegate)) {}

bool LinkHandlerModel::Init(content::BrowserContext* context, const GURL& url) {
  DCHECK(context);
  context_ = context;

  // Check if ARC apps can handle the |url|. Since the information is held in
  // a different (ARC) process, issue a mojo IPC request. Usually, the
  // callback function, OnUrlHandlerList, is called within a few milliseconds
  // even on the slowest Chromebook we support.
  url_ = RewriteUrlFromQueryIfAvailable(url);

  return mojo_delegate_->RequestUrlHandlerList(
      url_.spec(), base::BindOnce(&LinkHandlerModel::OnUrlHandlerList,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void LinkHandlerModel::OnUrlHandlerList(
    std::vector<ArcIntentHelperMojoDelegate::IntentHandlerInfo> handlers) {
  for (auto& handler : handlers) {
    if (handler.package_name == kArcIntentHelperPackageName) {
      continue;
    }
    handlers_.push_back(std::move(handler));
  }

  bool icon_info_notified = false;
  if (!ArcIconCacheDelegate::GetInstance()) {
    // ArcIconCacheDelegate instance should be already set on the product.
    // It is not set for some tests such as browser_tests since crosapi is
    // disabled. In this case, ignore the step to get icons and immediately
    // notify observers with no result.
    LOG(ERROR) << "ArcIconCacheDelegate is not set. "
               << "This should not happen except for testing.";
    NotifyObserver(nullptr);
    return;
  }

  std::vector<ArcIconCacheDelegate::ActivityName> activities;
  for (size_t i = 0; i < handlers_.size(); ++i) {
    activities.emplace_back(handlers_[i].package_name,
                            handlers_[i].activity_name);
  }
  const ArcIconCacheDelegate::GetResult result =
      ArcIconCacheDelegate::GetInstance()->GetActivityIcons(
          activities, base::BindOnce(&LinkHandlerModel::NotifyObserver,
                                     weak_ptr_factory_.GetWeakPtr()));
  icon_info_notified =
      ArcIconCacheDelegate::ActivityIconLoader::HasIconsReadyCallbackRun(
          result);

  if (!icon_info_notified) {
    // Call NotifyObserver() without icon information, unless
    // GetActivityIcons has already called it. Otherwise if we delay the
    // notification waiting for all icons, context menu may flicker.
    NotifyObserver(nullptr);
  }
}

void LinkHandlerModel::NotifyObserver(
    std::unique_ptr<ArcIconCacheDelegate::ActivityToIconsMap> icons) {
  if (icons) {
    icons_.insert(icons->begin(), icons->end());
    icons.reset();
  }

  std::vector<LinkHandlerInfo> handlers;
  for (size_t i = 0; i < handlers_.size(); ++i) {
    gfx::Image icon;
    const ArcIconCacheDelegate::ActivityName activity(
        handlers_[i].package_name, handlers_[i].activity_name);
    const auto it = icons_.find(activity);
    if (it != icons_.end()) {
      icon = it->second.icon16;
    }
    // Use the handler's index as an ID.
    LinkHandlerInfo handler = {base::UTF8ToUTF16(handlers_[i].name), icon,
                               static_cast<uint32_t>(i)};
    handlers.push_back(handler);
  }
  for (auto& observer : observer_list_) {
    observer.ModelChanged(handlers);
  }
}

// static
GURL LinkHandlerModel::RewriteUrlFromQueryIfAvailableForTesting(
    const GURL& url) {
  return RewriteUrlFromQueryIfAvailable(url);
}

// static
GURL LinkHandlerModel::RewriteUrlFromQueryIfAvailable(const GURL& url) {
  static const char kPathToFind[] = "/url";
  static const char kKeyToFind[] = "url";

  if (!google_util::IsGoogleDomainUrl(url, google_util::DISALLOW_SUBDOMAIN,
                                      google_util::ALLOW_NON_STANDARD_PORTS)) {
    return url;
  }
  if (!url.has_path() || url.path() != kPathToFind) {
    return url;
  }

  std::u16string value;
  if (!GetQueryValue(url, kKeyToFind, &value)) {
    return url;
  }

  const GURL new_url(value);
  if (!new_url.is_valid()) {
    return url;
  }
  return new_url;
}

}  // namespace arc
