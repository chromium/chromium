// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_AUDIO_ARC_AUDIO_BRIDGE_H_
#define COMPONENTS_ARC_AUDIO_ARC_AUDIO_BRIDGE_H_

#include <string>

#include "base/macros.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "components/arc/mojom/audio.mojom.h"
#include "components/arc/session/connection_observer.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

class ArcAudioBridge : public KeyedService,
                       public ConnectionObserver<mojom::AudioInstance>,
                       public mojom::AudioHost,
                       public chromeos::CrasAudioHandler::AudioObserver {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcAudioBridge* GetForBrowserContext(content::BrowserContext* context);

  ArcAudioBridge(content::BrowserContext* context,
                 ArcBridgeService* bridge_service);
  ~ArcAudioBridge() override;

  // ConnectionObserver<mojom::AudioInstance> overrides.
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // mojom::AudioHost overrides.
  void ShowVolumeControls() override;
  void OnSystemVolumeUpdateRequest(int32_t percent) override;

 private:
  // chromeos::CrasAudioHandler::AudioObserver overrides.
  void OnAudioNodesChanged() override;
  void OnOutputNodeVolumeChanged(uint64_t node_id, int volume) override;
  void OnOutputMuteChanged(bool mute_on) override;

  void SendSwitchState(bool headphone_inserted, bool microphone_inserted);
  void SendVolumeState();

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  chromeos::CrasAudioHandler* cras_audio_handler_;

  int volume_ = 0;  // Volume range: 0-100.
  bool muted_ = false;

  // Avoids sending requests when the instance is unavailable.
  // TODO(crbug.com/549195): Remove once the root cause is fixed.
  bool available_ = false;

  DISALLOW_COPY_AND_ASSIGN(ArcAudioBridge);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_AUDIO_ARC_AUDIO_BRIDGE_H_
