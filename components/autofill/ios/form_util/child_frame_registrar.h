// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_FORM_UTIL_CHILD_FRAME_REGISTRAR_H_
#define COMPONENTS_AUTOFILL_IOS_FORM_UTIL_CHILD_FRAME_REGISTRAR_H_

#import <map>
#import <optional>

#import "base/observer_list.h"
#import "base/scoped_multi_source_observation.h"
#import "base/scoped_observation.h"
#import "components/autofill/core/common/unique_ids.h"
#import "ios/web/public/js_messaging/java_script_feature.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace autofill {

// Observer of ChildFrameRegistrar events.
class ChildFrameRegistrarObserver : public base::CheckedObserver {
 public:
  // Called when there was an attempt made to register the same remote token
  // twice with a different |local| token. This may mean spoofing so the
  // receiver of this notification should act accordingly.
  virtual void OnDidDoubleRegistration(LocalFrameToken local) = 0;
};

// Child frame registration is the process whereby a frame can assign an ID (a
// remote frame token) to a child frame, establishing a relationship between
// that frame in the DOM (and JS) and the corresponding WebFrame object in C++.
// This class maintains those mappings.
class ChildFrameRegistrar : public web::WebStateUserData<ChildFrameRegistrar>,
                            public web::WebFramesManager::Observer,
                            public web::WebStateObserver {
 public:
  ~ChildFrameRegistrar() override;

  // Maps from remote to local tokens for all registered frames, to allow
  // lookup of a frame based on its remote token.
  //
  // Frame Tokens are used by browser-layer Autofill code to identify and
  // interact with a specific frame. Local Frame Tokens must not leak to frames
  // other than the ones they identify, while Remote Frame Tokens are also known
  // to the parent frame.
  //
  // In the context of iOS, the LocalFrameToken is equal to the frameId and can
  // be used to fetch the appropriate WebFrame from the WebFramesManager.
  std::optional<LocalFrameToken> LookupChildFrame(RemoteFrameToken remote);

  // Informs the Registrar that `remote` corresponds to the frame with frame ID
  // `local`.
  void RegisterMapping(RemoteFrameToken remote, LocalFrameToken local);

  // Convenience method that looks for dict values with keys "local_frame_id"
  // and "remote_frame_id" in the given `message`, converts them to the
  // appropriate tokens, and calls `RegisterMapping` using them. No-op if
  // `message` is not a dict, if the needed keys are not present, or if the
  // values are malformed.
  void ProcessRegistrationMessage(base::Value* message);

  // Notifies the registrar that a remote token `remote` has been dispatched to
  // a child frame. If the registrar already knows about this token, `callback`
  // will be invoked immediately with the corresponding local token. If not,
  // the registrar will hold on to the callback until it encounters `remote` and
  // execute it at that time.
  // Be careful what you bind to `callback`, and use WeakPtr as necessary, as
  // it may be retained indefinitely.
  void DeclareNewRemoteToken(
      RemoteFrameToken remote,
      base::OnceCallback<void(LocalFrameToken)> callback);

  // Returns the existing registrar for the given `web_state`, if there is one,
  // or creates a new one. May return nullptr if this functionality is not
  // available.
  static ChildFrameRegistrar* GetOrCreateForWebState(web::WebState* web_state);

  // Adds the |observer| to the list of observers.
  void AddObserver(ChildFrameRegistrarObserver* observer);

  // Removes |observer| from the list of observers.
  void RemoveObserver(ChildFrameRegistrarObserver* observer);

  // web::WebFramesManager::Observer:
  void WebFrameBecameUnavailable(web::WebFramesManager* web_frames_manager,
                                 const std::string& frame_id) override;

  // web::WebStateObserver
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  explicit ChildFrameRegistrar(web::WebState* web_state);
  friend class web::WebStateUserData<ChildFrameRegistrar>;
  WEB_STATE_USER_DATA_KEY_DECL();

  // Deletes all entries in `lookup_map_` that contain `frame_id` as local frame
  // token.
  void RemoveFrameID(const std::string& frame_id);

  // Maintains the mapping used by `LookupChildFrame`. The value containing the
  // local frame token is set to std::nullopt if there was at least one attempt
  // to register the same remote token, as a security precaution, making the
  // token and thus the frame invalid for the entire registrar's lifetime.
  std::map<RemoteFrameToken, std::optional<LocalFrameToken>> lookup_map_;

  // When `DeclareNewRemoteToken` is called, the RemoteFrameToken may not
  // yet correspond to a known frame. In this case, the callback is stored in
  // this map until a matching remote token is registered.
  std::map<RemoteFrameToken, base::OnceCallback<void(LocalFrameToken)>>
      pending_callbacks_;

  base::ObserverList<ChildFrameRegistrarObserver> observers_;

  // Observation for listening to WebFrame events. Multiple observations are
  // required because there are two WebFramesManager per WebState, one for each
  // ContentWorld.
  base::ScopedMultiSourceObservation<web::WebFramesManager,
                                     web::WebFramesManager::Observer>
      web_frames_managers_observation_{this};

  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_IOS_FORM_UTIL_CHILD_FRAME_REGISTRAR_H_
