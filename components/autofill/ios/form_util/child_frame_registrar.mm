// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/child_frame_registrar.h"

#import "base/values.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "components/autofill/ios/common/features.h"
#import "ios/web/public/js_messaging/content_world.h"

namespace autofill {

ChildFrameRegistrar::~ChildFrameRegistrar() = default;

std::optional<LocalFrameToken> ChildFrameRegistrar::LookupChildFrame(
    RemoteFrameToken remote) {
  if (lookup_map_.count(remote)) {
    return lookup_map_.at(remote);
  }
  return {};
}

void ChildFrameRegistrar::RegisterMapping(RemoteFrameToken remote,
                                          LocalFrameToken local) {
  // Make the currently registered local token invalid as re-registring
  // with the same unguessable |remote| token with a different |local| token may
  // mean a spoofing attack. Erase any pending callback that corresponds to the
  // remote token without running the callback. Notify about this event. This
  // also includes the case where the local token is already invalidated, which
  // will be no op.
  if (auto it = lookup_map_.find(remote); it != lookup_map_.end()) {
    std::optional<LocalFrameToken> current_local = it->second;

    pending_callbacks_.erase(remote);

    if ((current_local && current_local == local) || !current_local) {
      // Do not do anything else if the same `local` token was already
      // associated with the `remote` token. This isn't dangerous since this
      // must be coming from the same frame as before so doesn't fall as double
      // registration. Also, it is pointless to re-register since it doesn't
      // bring any new information to the system. The same applies if the
      // `current_local` was already invalidated where no further actions are
      // required.
      return;
    }

    // A different `local` token tries to register with the `remote` token, this
    // is double registration, invalidate the `remote` token to indicate that it
    // can't be used anymore.
    it->second = std::nullopt;

    // Notify that double registration happened and thus the |registered_local|
    // token was invalidated.
    for (auto& observer : observers_) {
      observer.OnDidDoubleRegistration(*current_local);
    }

    return;
  }

  // Proceed with registration.

  lookup_map_[remote] = local;

  // Check if we're waiting for this token and run the pending callback, if any.
  auto pending = pending_callbacks_.extract(remote);
  if (pending) {
    std::move(pending.mapped()).Run(local);
  }
}

void ChildFrameRegistrar::ProcessRegistrationMessage(base::Value* message) {
  base::Value::Dict* dict = message->GetIfDict();
  if (!dict) {
    return;
  }

  const std::string* local_frame_id = dict->FindString("local_frame_id");
  const std::string* remote_frame_id = dict->FindString("remote_frame_id");
  if (!local_frame_id || !remote_frame_id) {
    return;
  }

  std::optional<base::UnguessableToken> local =
      DeserializeJavaScriptFrameId(*local_frame_id);
  std::optional<base::UnguessableToken> remote =
      DeserializeJavaScriptFrameId(*remote_frame_id);

  if (!local || !remote) {
    return;
  }

  RegisterMapping(RemoteFrameToken(*remote), LocalFrameToken(*local));
}

void ChildFrameRegistrar::DeclareNewRemoteToken(
    RemoteFrameToken remote,
    base::OnceCallback<void(LocalFrameToken)> callback) {
  std::optional<LocalFrameToken> child_token = LookupChildFrame(remote);

  // If the child frame has already registered itself, we can set the parent
  // directly.
  if (child_token) {
    std::move(callback).Run(*child_token);
    return;
  }

  // Otherwise, store the relationship for later.
  pending_callbacks_[remote] = std::move(callback);
}

ChildFrameRegistrar* ChildFrameRegistrar::GetOrCreateForWebState(
    web::WebState* web_state) {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillAcrossIframesIos) &&
      !base::FeatureList::IsEnabled(kAutofillIsolatedWorldForJavascriptIos)) {
    return nullptr;
  }

  ChildFrameRegistrar* helper = FromWebState(web_state);
  if (!helper) {
    CreateForWebState(web_state);
    helper = FromWebState(web_state);
  }
  return helper;
}

// Adds the |observer| to the list of observers.
void ChildFrameRegistrar::AddObserver(ChildFrameRegistrarObserver* observer) {
  observers_.AddObserver(observer);
}

// Removes |observer| from the list of observers.
void ChildFrameRegistrar::RemoveObserver(
    ChildFrameRegistrarObserver* observer) {
  observers_.RemoveObserver(observer);
}

ChildFrameRegistrar::ChildFrameRegistrar(web::WebState* web_state) {
  CHECK(web_state);

  // Monitor frame destruction. When frames are gone we clean up entries in
  // `lookup_map_` containing their local frame token.
  for (auto content_world : {web::ContentWorld::kPageContentWorld,
                             web::ContentWorld::kIsolatedWorld}) {
    web_frames_managers_observation_.AddObservation(
        web_state->GetWebFramesManager(content_world));
  }
  // On WebState destruction we need to remove the frame managers observation.
  // Otherwise the frame managers can be destroyed first which cause a UAF when
  // `this` is destroyed and tries to remove itself as an observer of the
  // destroyed frame manager.
  web_state_observation_.Observe(web_state);
}

void ChildFrameRegistrar::WebFrameBecameUnavailable(
    web::WebFramesManager* web_frames_manager,
    const std::string& frame_id) {
  // Now that the frame with `frame_id` is gone, remove all stale entries in
  // `lookup_map_` containing `frame_id` as local frame token.
  RemoveFrameID(frame_id);
}

void ChildFrameRegistrar::WebStateDestroyed(web::WebState* web_state) {
  web_frames_managers_observation_.RemoveAllObservations();
  web_state_observation_.Reset();
}

void ChildFrameRegistrar::RemoveFrameID(const std::string& frame_id) {
  std::optional<base::UnguessableToken> deserialized_frame_id =
      DeserializeJavaScriptFrameId(frame_id);
  if (!deserialized_frame_id) {
    return;
  }
  LocalFrameToken local_frame_token = LocalFrameToken(*deserialized_frame_id);

  for (auto it = lookup_map_.begin(); it != lookup_map_.end();) {
    if (it->second == local_frame_token) {
      lookup_map_.erase(it++);
    } else {
      ++it;
    }
  }
}

WEB_STATE_USER_DATA_KEY_IMPL(ChildFrameRegistrar)

}  // namespace autofill
