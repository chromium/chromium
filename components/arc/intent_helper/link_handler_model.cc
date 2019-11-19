// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/link_handler_model.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/metrics/arc_metrics_constants.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/google/core/common/google_util.h"
#include "url/url_util.h"

namespace arc {

namespace {

constexpr int kMaxValueLen = 2048;

bool GetQueryValue(const GURL& url,
                   const std::string& key_to_find,
                   base::string16* out) {
  const std::string str(url.query());

  url::Component query(0, str.length());
  url::Component key;
  url::Component value;

  while (url::ExtractQueryKeyValue(str.c_str(), &query, &key, &value)) {
    if (!value.is_nonempty())
      continue;
    if (str.substr(key.begin, key.len) == key_to_find) {
      if (value.len >= kMaxValueLen)
        return false;
      url::RawCanonOutputW<kMaxValueLen> output;
      url::DecodeURLEscapeSequences(str.c_str() + value.begin, value.len,
                                    url::DecodeURLMode::kUTF8OrIsomorphic,
                                    &output);
      *out = base::string16(output.data(), output.length());
      return true;
    }
  }
  return false;
}

}  // namespace

// static
std::unique_ptr<LinkHandlerModel> LinkHandlerModel::Create(
    content::BrowserContext* context,
    const GURL& link_url) {
  auto impl = base::WrapUnique(new LinkHandlerModel());
  if (!impl->Init(context, link_url))
    return nullptr;
  return impl;
}

LinkHandlerModel::~LinkHandlerModel() = default;

void LinkHandlerModel::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void LinkHandlerModel::OpenLinkWithHandler(uint32_t handler_id) {
  auto* arc_service_manager = ArcServiceManager::Get();
  if (!arc_service_manager)
    return;
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(), HandleUrl);
  if (!instance)
    return;
  if (handler_id >= handlers_.size())
    return;
  instance->HandleUrl(url_.spec(), handlers_[handler_id]->package_name);

  UMA_HISTOGRAM_ENUMERATION(
      "Arc.UserInteraction",
      arc::UserInteractionType::APP_STARTED_FROM_LINK_CONTEXT_MENU);
}

LinkHandlerModel::LinkHandlerModel() = default;

bool LinkHandlerModel::Init(content::BrowserContext* context, const GURL& url) {
  auto* arc_service_manager = ArcServiceManager::Get();
  if (!arc_service_manager)
    return false;
  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(),
      RequestUrlHandlerList);
  if (!instance)
    return false;

  DCHECK(context);
  context_ = context;

  // Check if ARC apps can handle the |url|. Since the information is held in
  // a different (ARC) process, issue a mojo IPC request. Usually, the
  // callback function, OnUrlHandlerList, is called within a few milliseconds
  // even on the slowest Chromebook we support.
  url_ = RewriteUrlFromQueryIfAvailable(url);
  instance->RequestUrlHandlerList(
      url_.spec(), base::BindOnce(&LinkHandlerModel::OnUrlHandlerList,
                                  weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void LinkHandlerModel::OnUrlHandlerList(
    std::vector<mojom::IntentHandlerInfoPtr> handlers) {
  handlers_ = ArcIntentHelperBridge::FilterOutIntentHelper(std::move(handlers));

  bool icon_info_notified = false;
  auto* intent_helper_bridge =
      ArcIntentHelperBridge::GetForBrowserContext(context_);
  if (intent_helper_bridge) {
    std::vector<ArcIntentHelperBridge::ActivityName> activities;
    for (size_t i = 0; i < handlers_.size(); ++i) {
      activities.emplace_back(handlers_[i]->package_name,
                              handlers_[i]->activity_name);
    }
    const ArcIntentHelperBridge::GetResult result =
        intent_helper_bridge->GetActivityIcons(
            activities, base::BindOnce(&LinkHandlerModel::NotifyObserver,
                                       weak_ptr_factory_.GetWeakPtr()));
    icon_info_notified =
        internal::ActivityIconLoader::HasIconsReadyCallbackRun(result);
  }

  if (!icon_info_notified) {
    // Call NotifyObserver() without icon information, unless
    // GetActivityIcons has already called it. Otherwise if we delay the
    // notification waiting for all icons, context menu may flicker.
    NotifyObserver(nullptr);
  }
}

void LinkHandlerModel::NotifyObserver(
    std::unique_ptr<ArcIntentHelperBridge::ActivityToIconsMap> icons) {
  if (icons) {
    icons_.insert(icons->begin(), icons->end());
    icons.reset();
  }

  std::vector<LinkHandlerInfo> handlers;
  for (size_t i = 0; i < handlers_.size(); ++i) {
    gfx::Image icon;
    const ArcIntentHelperBridge::ActivityName activity(
        handlers_[i]->package_name, handlers_[i]->activity_name);
    const auto it = icons_.find(activity);
    if (it != icons_.end())
      icon = it->second.icon16;
    // Use the handler's index as an ID.
    LinkHandlerInfo handler = {base::UTF8ToUTF16(handlers_[i]->name), icon, i};
    handlers.push_back(handler);
  }
  for (auto& observer : observer_list_)
    observer.ModelChanged(handlers);
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

  if (!google_util::IsGoogleHostname(url.host_piece(),
                                     google_util::DISALLOW_SUBDOMAIN)) {
    return url;
  }
  if (!url.has_path() || url.path() != kPathToFind)
    return url;

  base::string16 value;
  if (!GetQueryValue(url, kKeyToFind, &value))
    return url;

  const GURL new_url(value);
  if (!new_url.is_valid())
    return url;
  return new_url;
}

}  // namespace arc
