// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MEDIA_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_MEDIA_CLIENT_IMPL_H_

#include "ash/public/cpp/media_client.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "ui/base/accelerators/media_keys_listener.h"

namespace ash {
enum class MediaCaptureState;
class MediaController;
}  // namespace ash

class MediaClientImpl : public ash::MediaClient,
                        public BrowserListObserver,
                        public MediaCaptureDevicesDispatcher::Observer {
 public:
  MediaClientImpl();
  ~MediaClientImpl() override;

  // Initializes and set as client for ash.
  void Init();

  // Tests can provide a mock mojo interface for the ash controller.
  void InitForTesting(ash::MediaController* controller);

  // Returns a pointer to the singleton MediaClient, or nullptr if none exists.
  static MediaClientImpl* Get();

  // ash::MediaClient:
  void HandleMediaNextTrack() override;
  void HandleMediaPlayPause() override;
  void HandleMediaPrevTrack() override;
  void RequestCaptureState() override;
  void SuspendMediaSessions() override;

  // MediaCaptureDevicesDispatcher::Observer:
  void OnRequestUpdate(int render_process_id,
                       int render_frame_id,
                       blink::mojom::MediaStreamType stream_type,
                       const content::MediaRequestState state) override;

  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;

  // Enables/disables custom media key handling when |context| is the active
  // browser. Media keys will be forwarded to |delegate|.
  void EnableCustomMediaKeyHandler(content::BrowserContext* context,
                                   ui::MediaKeysListener::Delegate* delegate);
  void DisableCustomMediaKeyHandler(content::BrowserContext* context,
                                    ui::MediaKeysListener::Delegate* delegate);

 private:
  // Sets |is_forcing_media_client_key_handling_| to true if
  // |GetCurrentMediaKeyDelegate| returns a delegate. This will also mirror the
  // value of |is_forcing_media_client_key_handling_| to Ash.
  void UpdateForceMediaClientKeyHandling();

  // Gets the current media key delegate that we should forward media keys to.
  // This will be the delegate that is associated with the current active
  // browser. If there is no delegate registered or there is no active browser
  // then this will return |nullptr|.
  ui::MediaKeysListener::Delegate* GetCurrentMediaKeyDelegate() const;

  // Returns the media capture state for the current user at
  // |user_index|. (Note that this isn't stable, see implementation comment on
  // RequestCaptureState()).
  ash::MediaCaptureState GetMediaCaptureStateByIndex(int user_index);

  // Handles the media key action for the key with |code|. If there is a
  // |GetCurrentMediaKeyDelegate| then the action will be forwarded to the
  // delegate. Otherwise, we will forward the action to the extensions API.
  void HandleMediaAction(ui::KeyboardCode code);

  ash::MediaController* media_controller_ = nullptr;

  base::flat_map<content::BrowserContext*, ui::MediaKeysListener::Delegate*>
      media_key_delegates_;

  // If true then ash will always forward media keys to |this| instead of trying
  // to handle them first.
  bool is_forcing_media_client_key_handling_ = false;

  content::BrowserContext* active_context_ = nullptr;

  base::WeakPtrFactory<MediaClientImpl> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaClientImpl);
};

#endif  // CHROME_BROWSER_UI_ASH_MEDIA_CLIENT_IMPL_H_
