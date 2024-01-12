// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/form_util/child_frame_registrar.h"

#import "base/values.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/browser/autofill_util.h"

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
  // TODO(crbug.com/1440471): Handle double registration
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
          autofill::features::kAutofillAcrossIframesIos)) {
    return nullptr;
  }

  ChildFrameRegistrar* helper = FromWebState(web_state);
  if (!helper) {
    CreateForWebState(web_state);
    helper = FromWebState(web_state);
  }
  return helper;
}

ChildFrameRegistrar::ChildFrameRegistrar(web::WebState* web_state) {
  // TODO(crbug.com/1440471): Register self as observer of WebState and remove
  // stale frame IDs from `lookup_map_`.
}

WEB_STATE_USER_DATA_KEY_IMPL(ChildFrameRegistrar)

}  // namespace autofill
