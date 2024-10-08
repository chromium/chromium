// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/test/fake_intent_helper_instance.h"

#include <iterator>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"

namespace arc {

FakeIntentHelperInstance::FakeIntentHelperInstance() = default;

FakeIntentHelperInstance::Broadcast::Broadcast(const std::string& action,
                                               const std::string& package_name,
                                               const std::string& cls,
                                               const std::string& extras)
    : action(action), package_name(package_name), cls(cls), extras(extras) {}

FakeIntentHelperInstance::Broadcast::Broadcast(const Broadcast& broadcast)
    : action(broadcast.action),
      package_name(broadcast.package_name),
      cls(broadcast.cls),
      extras(broadcast.extras) {}

FakeIntentHelperInstance::Broadcast::~Broadcast() {}

FakeIntentHelperInstance::HandledIntent::HandledIntent(
    mojom::IntentInfoPtr intent,
    mojom::ActivityNamePtr activity)
    : intent(std::move(intent)), activity(std::move(activity)) {}

FakeIntentHelperInstance::HandledIntent::HandledIntent(HandledIntent&& other) =
    default;

FakeIntentHelperInstance::HandledIntent&
FakeIntentHelperInstance::HandledIntent::operator=(HandledIntent&& other) =
    default;

FakeIntentHelperInstance::HandledIntent::~HandledIntent() = default;

void FakeIntentHelperInstance::SetIntentHandlers(
    const std::string& action,
    std::vector<mojom::IntentHandlerInfoPtr> handlers) {
  intent_handlers_[action] = std::move(handlers);
}

FakeIntentHelperInstance::~FakeIntentHelperInstance() = default;

void FakeIntentHelperInstance::AddPreferredPackage(
    const std::string& package_name) {}

void FakeIntentHelperInstance::SetVerifiedLinks(
    const std::vector<std::string>& package_names,
    bool always_open) {
  for (const auto& package : package_names) {
    verified_links_[package] = always_open;
  }
}

void FakeIntentHelperInstance::HandleIntent(mojom::IntentInfoPtr intent,
                                            mojom::ActivityNamePtr activity) {
  handled_intents_.emplace_back(std::move(intent), std::move(activity));
}

void FakeIntentHelperInstance::HandleIntentWithWindowInfo(
    mojom::IntentInfoPtr intent,
    mojom::ActivityNamePtr activity,
    mojom::WindowInfoPtr window_info) {
  handled_intents_.emplace_back(std::move(intent), std::move(activity));
}

void FakeIntentHelperInstance::HandleUrl(const std::string& url,
                                         const std::string& package_name) {}

void FakeIntentHelperInstance::Init(
    mojo::PendingRemote<mojom::IntentHelperHost> host_remote,
    InitCallback callback) {
  // For every change in a connection bind latest remote.
  host_remote_.reset();
  host_remote_.Bind(std::move(host_remote));
  std::move(callback).Run();
}

void FakeIntentHelperInstance::RequestActivityIcons(
    std::vector<mojom::ActivityNamePtr> activities,
    ::arc::mojom::ScaleFactor scale_factor,
    RequestActivityIconsCallback callback) {}

void FakeIntentHelperInstance::RequestIntentHandlerList(
    mojom::IntentInfoPtr intent,
    RequestIntentHandlerListCallback callback) {
  std::vector<mojom::IntentHandlerInfoPtr> handlers;
  const auto it = intent_handlers_.find(intent->action);
  if (it != intent_handlers_.end()) {
    handlers.reserve(it->second.size());
    for (const auto& handler : it->second) {
      handlers.emplace_back(handler.Clone());
    }
  }
  // Post the reply to run asynchronously to match the real implementation.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(handlers)));
}

void FakeIntentHelperInstance::RequestUrlHandlerList(
    const std::string& url,
    RequestUrlHandlerListCallback callback) {
  std::vector<mojom::IntentHandlerInfoPtr> handlers;
  // Post the reply to run asynchronously to match the real implementation.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(handlers)));
}

void FakeIntentHelperInstance::RequestUrlListHandlerList(
    std::vector<mojom::UrlWithMimeTypePtr> urls,
    RequestUrlListHandlerListCallback callback) {}

void FakeIntentHelperInstance::SendBroadcast(const std::string& action,
                                             const std::string& package_name,
                                             const std::string& cls,
                                             const std::string& extras) {
  broadcasts_.emplace_back(action, package_name, cls, extras);
}

void FakeIntentHelperInstance::RequestTextSelectionActions(
    const std::string& text,
    ::arc::mojom::ScaleFactor scale_factor,
    RequestTextSelectionActionsCallback callback) {}

void FakeIntentHelperInstance::HandleCameraResult(
    uint32_t intent_id,
    arc::mojom::CameraIntentAction action,
    const std::vector<uint8_t>& data,
    HandleCameraResultCallback callback) {}

std::vector<FakeIntentHelperInstance::Broadcast>
FakeIntentHelperInstance::GetBroadcastsForAction(
    const std::string& action) const {
  std::vector<Broadcast> result;
  base::ranges::copy_if(
      broadcasts_, std::back_inserter(result),
      [&action](const Broadcast& b) { return b.action == action; });
  return result;
}

void FakeIntentHelperInstance::RequestDomainVerificationStatusUpdate() {}

void FakeIntentHelperInstance::SetCaptionStyle(
    arc::mojom::CaptionStylePtr caption_style) {
  caption_style_ = std::move(caption_style);
}

void FakeIntentHelperInstance::EnableAccessibilityFeatures(
    arc::mojom::AccessibilityFeaturesPtr accessibility_features) {
  accessibility_features_ = std::move(accessibility_features);
}

}  // namespace arc
