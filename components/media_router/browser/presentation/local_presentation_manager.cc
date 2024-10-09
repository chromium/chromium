// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/presentation/local_presentation_manager.h"

#include <utility>

#include "base/containers/contains.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

using blink::mojom::PresentationConnectionResult;
using blink::mojom::PresentationInfo;

namespace media_router {

// LocalPresentationManager implementation.
LocalPresentationManager::LocalPresentationManager() = default;

LocalPresentationManager::~LocalPresentationManager() = default;

LocalPresentationManager::LocalPresentation*
LocalPresentationManager::GetOrCreateLocalPresentation(
    const PresentationInfo& presentation_info) {
  auto it = local_presentations_.find(presentation_info.id);
  if (it == local_presentations_.end()) {
    it = local_presentations_
             .insert(std::make_pair(
                 presentation_info.id,
                 std::make_unique<LocalPresentation>(presentation_info)))
             .first;
  }
  return it->second.get();
}

void LocalPresentationManager::RegisterLocalPresentationController(
    const PresentationInfo& presentation_info,
    const content::GlobalRenderFrameHostId& render_frame_host_id,
    mojo::PendingRemote<blink::mojom::PresentationConnection>
        controller_connection_remote,
    mojo::PendingReceiver<blink::mojom::PresentationConnection>
        receiver_connection_receiver,
    const MediaRoute& route) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* presentation = GetOrCreateLocalPresentation(presentation_info);
  presentation->RegisterController(
      render_frame_host_id, std::move(controller_connection_remote),
      std::move(receiver_connection_receiver), route);
}

void LocalPresentationManager::UnregisterLocalPresentationController(
    const std::string& presentation_id,
    const content::GlobalRenderFrameHostId& render_frame_host_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto it = local_presentations_.find(presentation_id);
  if (it == local_presentations_.end())
    return;

  // Remove presentation if no controller and receiver.
  it->second->UnregisterController(render_frame_host_id);
  if (!it->second->IsValid()) {
    local_presentations_.erase(presentation_id);
  }
}

void LocalPresentationManager::OnLocalPresentationReceiverCreated(
    const PresentationInfo& presentation_info,
    const content::ReceiverConnectionAvailableCallback& receiver_callback,
    content::WebContents* receiver_web_contents) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* presentation = GetOrCreateLocalPresentation(presentation_info);
  presentation->RegisterReceiver(receiver_callback, receiver_web_contents);
}

void LocalPresentationManager::OnLocalPresentationReceiverTerminated(
    const std::string& presentation_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  local_presentations_.erase(presentation_id);
}

bool LocalPresentationManager::IsLocalPresentation(
    const std::string& presentation_id) {
  return base::Contains(local_presentations_, presentation_id);
}

bool LocalPresentationManager::IsLocalPresentation(
    content::WebContents* web_contents) {
  for (auto& local_presentation : local_presentations_) {
    if (local_presentation.second->receiver_web_contents_ == web_contents)
      return true;
  }
  return false;
}

const MediaRoute* LocalPresentationManager::GetRoute(
    const std::string& presentation_id) {
  auto it = local_presentations_.find(presentation_id);
  return (it != local_presentations_.end() && it->second->route_.has_value())
             ? &(it->second->route_.value())
             : nullptr;
}

// LocalPresentation implementation.
LocalPresentationManager::LocalPresentation::LocalPresentation(
    const PresentationInfo& presentation_info)
    : presentation_info_(presentation_info) {}

LocalPresentationManager::LocalPresentation::~LocalPresentation() {}

void LocalPresentationManager::LocalPresentation::RegisterController(
    const content::GlobalRenderFrameHostId& render_frame_host_id,
    mojo::PendingRemote<blink::mojom::PresentationConnection>
        controller_connection_remote,
    mojo::PendingReceiver<blink::mojom::PresentationConnection>
        receiver_connection_receiver,
    const MediaRoute& route) {
  if (!receiver_callback_.is_null()) {
    receiver_callback_.Run(PresentationConnectionResult::New(
        PresentationInfo::New(presentation_info_),
        std::move(controller_connection_remote),
        std::move(receiver_connection_receiver)));
  } else {
    pending_controllers_.insert(std::make_pair(
        render_frame_host_id, std::make_unique<ControllerConnection>(
                                  std::move(controller_connection_remote),
                                  std::move(receiver_connection_receiver))));
  }
  route_ = route;
}

void LocalPresentationManager::LocalPresentation::UnregisterController(
    const content::GlobalRenderFrameHostId& render_frame_host_id) {
  pending_controllers_.erase(render_frame_host_id);
}

void LocalPresentationManager::LocalPresentation::RegisterReceiver(
    const content::ReceiverConnectionAvailableCallback& receiver_callback,
    content::WebContents* receiver_web_contents) {
  DCHECK(receiver_callback_.is_null());
  DCHECK(receiver_web_contents);
  for (auto& controller : pending_controllers_) {
    receiver_callback.Run(PresentationConnectionResult::New(
        PresentationInfo::New(presentation_info_),
        std::move(controller.second->controller_connection_remote),
        std::move(controller.second->receiver_connection_receiver)));
  }
  receiver_callback_ = receiver_callback;
  receiver_web_contents_ = receiver_web_contents;
  pending_controllers_.clear();
}

bool LocalPresentationManager::LocalPresentation::IsValid() const {
  return !(pending_controllers_.empty() && receiver_callback_.is_null());
}

LocalPresentationManager::LocalPresentation::ControllerConnection::
    ControllerConnection(
        mojo::PendingRemote<blink::mojom::PresentationConnection>
            controller_connection_remote,
        mojo::PendingReceiver<blink::mojom::PresentationConnection>
            receiver_connection_receiver)
    : controller_connection_remote(std::move(controller_connection_remote)),
      receiver_connection_receiver(std::move(receiver_connection_receiver)) {}

LocalPresentationManager::LocalPresentation::ControllerConnection::
    ~ControllerConnection() = default;

}  // namespace media_router
