// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_LIBASSISTANT_TEST_SUPPORT_LIBASSISTANT_SERVICE_TESTER_H_
#define CHROMEOS_SERVICES_LIBASSISTANT_TEST_SUPPORT_LIBASSISTANT_SERVICE_TESTER_H_

#include "base/test/scoped_path_override.h"
#include "chromeos/services/assistant/public/cpp/migration/fake_assistant_manager_service_delegate.h"
#include "chromeos/services/libassistant/libassistant_service.h"
#include "chromeos/services/libassistant/public/mojom/audio_input_controller.mojom.h"
#include "chromeos/services/libassistant/public/mojom/audio_output_delegate.mojom-forward.h"
#include "chromeos/services/libassistant/public/mojom/conversation_controller.mojom.h"
#include "chromeos/services/libassistant/public/mojom/display_controller.mojom.h"
#include "chromeos/services/libassistant/public/mojom/service.mojom.h"
#include "chromeos/services/libassistant/public/mojom/service_controller.mojom.h"
#include "chromeos/services/libassistant/public/mojom/speaker_id_enrollment_controller.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace assistant {
class FakeAssistantManager;
class FakeAssistantManagerInternal;
}  // namespace assistant
}  // namespace chromeos

namespace chromeos {
namespace libassistant {

// Helper class that makes it easier to test |LibassistantService|.
class LibassistantServiceTester {
 public:
  LibassistantServiceTester();
  LibassistantServiceTester(const LibassistantServiceTester&) = delete;
  LibassistantServiceTester& operator=(const LibassistantServiceTester&) =
      delete;
  ~LibassistantServiceTester();

  // Initialize and start Libassistant.
  void Start();

  LibassistantService& service() { return service_; }

  assistant::FakeAssistantManager& assistant_manager() {
    return *assistant_manager_service_delegate_.assistant_manager();
  }

  assistant::FakeAssistantManagerInternal& assistant_manager_internal() {
    return *assistant_manager_service_delegate_.assistant_manager_internal();
  }

  mojom::AudioInputController& audio_input_controller() {
    return *audio_input_controller_.get();
  }
  mojom::ConversationController& conversation_controller() {
    return *conversation_controller_.get();
  }
  mojom::DisplayController& display_controller() {
    return *display_controller_.get();
  }
  mojom::ServiceController& service_controller() {
    return *service_controller_.get();
  }
  mojom::SpeakerIdEnrollmentController& speaker_id_enrollment_controller() {
    return *speaker_id_enrollment_controller_.get();
  }

  void FlushForTesting();

 private:
  void BindControllers();

  mojo::Remote<mojom::AudioInputController> audio_input_controller_;
  mojo::Remote<mojom::ConversationController> conversation_controller_;
  mojo::Remote<mojom::DisplayController> display_controller_;
  mojo::Remote<mojom::MediaController> media_controller_;
  mojo::Remote<mojom::ServiceController> service_controller_;
  mojo::Remote<mojom::SpeakerIdEnrollmentController>
      speaker_id_enrollment_controller_;
  mojo::PendingReceiver<mojom::AudioOutputDelegate>
      pending_audio_output_delegate_;
  mojo::PendingReceiver<mojom::MediaDelegate> pending_media_delegate_;
  mojo::PendingReceiver<mojom::PlatformDelegate> pending_platform_delegate_;

  mojo::Remote<mojom::LibassistantService> service_remote_;
  assistant::FakeAssistantManagerServiceDelegate
      assistant_manager_service_delegate_;
  // Our file provider requires the home dir to be overridden.
  base::ScopedPathOverride home_dir_override_;
  LibassistantService service_;
};

}  // namespace libassistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_LIBASSISTANT_TEST_SUPPORT_LIBASSISTANT_SERVICE_TESTER_H_
