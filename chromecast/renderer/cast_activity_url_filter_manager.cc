// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_activity_url_filter_manager.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace chromecast {

CastActivityUrlFilterManager::UrlFilterReceiver::UrlFilterReceiver(
    content::RenderFrame* render_frame,
    base::OnceCallback<void()> on_removed_callback)
    : content::RenderFrameObserver(render_frame),
      on_removed_callback_(std::move(on_removed_callback)),
      weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
  render_frame->GetAssociatedInterfaceRegistry()
      ->AddInterface<chromecast::mojom::ActivityUrlFilterConfiguration>(
          base::BindRepeating(
              &CastActivityUrlFilterManager::UrlFilterReceiver::
                  OnActivityUrlFilterConfigurationAssociatedRequest,
              weak_this_));
}

CastActivityUrlFilterManager::UrlFilterReceiver::~UrlFilterReceiver() {
  std::move(on_removed_callback_).Run();
}

bool CastActivityUrlFilterManager::UrlFilterReceiver::
    OnAssociatedInterfaceRequestForFrame(
        const std::string& interface_name,
        mojo::ScopedInterfaceEndpointHandle* handle) {
  return associated_interfaces_.TryBindInterface(interface_name, handle);
}

void CastActivityUrlFilterManager::UrlFilterReceiver::OnDestruct() {
  delete this;
}

void CastActivityUrlFilterManager::UrlFilterReceiver::SetFilter(
    chromecast::mojom::ActivityUrlFilterCriteriaPtr filter_criteria) {
  if (filter_criteria->criteria.empty())
    return;

  url_filter_ = std::make_unique<ActivityUrlFilter>(filter_criteria->criteria);
}

void CastActivityUrlFilterManager::UrlFilterReceiver::
    OnActivityUrlFilterConfigurationAssociatedRequest(
        mojo::PendingAssociatedReceiver<
            chromecast::mojom::ActivityUrlFilterConfiguration> receiver) {
  receivers_.Add(this, std::move(receiver));
}

ActivityUrlFilter*
CastActivityUrlFilterManager::UrlFilterReceiver::GetUrlFilter() {
  return url_filter_.get();
}

CastActivityUrlFilterManager::CastActivityUrlFilterManager()
    : weak_factory_(this) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

CastActivityUrlFilterManager::~CastActivityUrlFilterManager() = default;

ActivityUrlFilter*
CastActivityUrlFilterManager::GetActivityUrlFilterForRenderFrameToken(
    const blink::LocalFrameToken& frame_token) {
  const auto& it = activity_url_filters_.find(frame_token);
  if (it == activity_url_filters_.end())
    return nullptr;

  return it->second->GetUrlFilter();
}

void CastActivityUrlFilterManager::OnRenderFrameCreated(
    content::RenderFrame* render_frame) {
  auto frame_token = render_frame->GetWebFrame()->GetLocalFrameToken();

  // Lifetime is tied to |render_frame| via content::RenderFrameObserver.
  auto* filter_receiver = new CastActivityUrlFilterManager::UrlFilterReceiver(
      render_frame,
      base::BindOnce(&CastActivityUrlFilterManager::OnRenderFrameRemoved,
                     weak_this_, frame_token));

  auto result = activity_url_filters_.emplace(frame_token, filter_receiver);

  if (!result.second)
    LOG(ERROR)
        << "A URL filter for Activity already exists for Render frame token "
        << frame_token;
}

void CastActivityUrlFilterManager::OnRenderFrameRemoved(
    const blink::LocalFrameToken& frame_token) {
  const auto& it = activity_url_filters_.find(frame_token);

  if (it != activity_url_filters_.end())
    activity_url_filters_.erase(it);
}

}  // namespace chromecast
