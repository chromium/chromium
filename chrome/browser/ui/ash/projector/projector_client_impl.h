// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_CLIENT_IMPL_H_
#define CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_CLIENT_IMPL_H_

#include <memory>

#include "ash/public/cpp/projector/projector_client.h"
#include "ash/public/cpp/projector/projector_controller.h"
#include "ash/public/cpp/projector/speech_recognition_availability.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/speech/speech_recognizer_delegate.h"
#include "chrome/browser/ui/ash/projector/projector_drivefs_provider.h"
#include "chrome/browser/ui/ash/projector/projector_soda_installation_controller.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

class SpeechRecognitionRecognizerClientImpl;

// The client implementation for the ProjectorController in ash/. This client is
// responsible for handling requests that have browser dependencies.
class ProjectorClientImpl : public ash::ProjectorClient,
                            public SpeechRecognizerDelegate,
                            drive::DriveIntegrationService::Observer,
                            session_manager::SessionManagerObserver {
 public:

  explicit ProjectorClientImpl(ash::ProjectorController* controller);

  ProjectorClientImpl();
  ProjectorClientImpl(const ProjectorClientImpl&) = delete;
  ProjectorClientImpl& operator=(const ProjectorClientImpl&) = delete;
  ~ProjectorClientImpl() override;

  // ash::ProjectorClient:
  ash::SpeechRecognitionAvailability GetSpeechRecognitionAvailability()
      const override;
  void StartSpeechRecognition() override;
  void StopSpeechRecognition() override;
  void ForceEndSpeechRecognition() override;
  bool GetBaseStoragePath(base::FilePath* result) const override;
  bool IsDriveFsMounted() const override;
  bool IsDriveFsMountFailed() const override;
  void OpenProjectorApp() const override;
  void MinimizeProjectorApp() const override;
  void CloseProjectorApp() const override;
  void OnNewScreencastPreconditionChanged(
      const ash::NewScreencastPrecondition& precondition) const override;
  void ToggleFileSyncingNotificationForPaths(
      const std::vector<base::FilePath>& screencast_paths,
      bool suppress) override;

  // SpeechRecognizerDelegate:
  void OnSpeechResult(
      const std::u16string& text,
      bool is_final,
      const std::optional<media::SpeechRecognitionResult>& timing) override;
  // This class is not utilizing the information about sound level.
  void OnSpeechSoundLevelChanged(int16_t level) override {}
  void OnSpeechRecognitionStateChanged(
      SpeechRecognizerStatus new_state) override;
  void OnSpeechRecognitionStopped() override;
  void OnLanguageIdentificationEvent(
      media::mojom::LanguageIdentificationEventPtr event) override;

  // DriveIntegrationService::Observer implementation.
  void OnFileSystemMounted() override;
  void OnFileSystemBeingUnmounted() override;
  void OnFileSystemMountFailed() override;

  // session_manager::SessionManagerObserver:
  void OnUserSessionStarted(bool is_primary_user) override;

  // Maybe observe the Drive integration service of active profile when
  // ActiveUserChanged and OnUserProfileLoaded.
  void MaybeSwitchDriveIntegrationServiceObservation();

 private:
  void SpeechRecognitionEnded(bool forced);

  // Called when any of the policies change that control whether the Projector
  // app is enabled.
  void OnEnablementPolicyChanged();

  // Called when app registry becomes ready.
  void SetAppIsDisabled(bool disabled);

  const raw_ptr<ash::ProjectorController> controller_;
  SpeechRecognizerStatus recognizer_status_ =
      SpeechRecognizerStatus::SPEECH_RECOGNIZER_OFF;
  std::unique_ptr<SpeechRecognitionRecognizerClientImpl> speech_recognizer_;

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};

  PrefChangeRegistrar pref_change_registrar_;

  ProjectorDriveFsProvider drive_helper_;

  std::unique_ptr<ProjectorSodaInstallationController>
      soda_installation_controller_;

  base::WeakPtrFactory<ProjectorClientImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_PROJECTOR_PROJECTOR_CLIENT_IMPL_H_
