// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_VIEWS_CAPTION_BUBBLE_CONTROLLER_VIEWS_H_
#define COMPONENTS_LIVE_CAPTION_VIEWS_CAPTION_BUBBLE_CONTROLLER_VIEWS_H_

#include <memory>
#include <set>
#include <string>
#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/live_caption/caption_bubble_controller.h"
#include "components/live_caption/views/caption_bubble.h"
#include "components/prefs/pref_service.h"
#include "components/soda/soda_installer.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"

namespace views {
class Widget;
}

namespace captions {

class CaptionBubble;
class CaptionBubbleModel;
class CaptionBubbleSessionObserver;

///////////////////////////////////////////////////////////////////////////////
// Caption Bubble Controller for Views
//
//  The implementation of the caption bubble controller for Views.
//
class CaptionBubbleControllerViews : public CaptionBubbleController,
                                     public speech::SodaInstaller::Observer {
 public:
  CaptionBubbleControllerViews(PrefService* profile_prefs,
                               const std::string& application_locale);
  ~CaptionBubbleControllerViews() override;
  CaptionBubbleControllerViews(const CaptionBubbleControllerViews&) = delete;
  CaptionBubbleControllerViews& operator=(const CaptionBubbleControllerViews&) =
      delete;

  // Called when a transcription is received from the service. Returns whether
  // the transcription result was set on the caption bubble successfully.
  // Transcriptions will halt if this returns false.
  bool OnTranscription(CaptionBubbleContext* caption_bubble_context,
                       const media::SpeechRecognitionResult& result) override;

  // Called when the speech service has an error.
  void OnError(
      CaptionBubbleContext* caption_bubble_context,
      CaptionBubbleErrorType error_type,
      OnErrorClickedCallback error_clicked_callback,
      OnDoNotShowAgainClickedCallback error_silenced_callback) override;

  // Called when the audio stream has ended.
  void OnAudioStreamEnd(CaptionBubbleContext* caption_bubble_context) override;

  // Called when the caption style changes.
  void UpdateCaptionStyle(
      std::optional<ui::CaptionStyle> caption_style) override;

  bool IsWidgetVisibleForTesting() override;
  bool IsGenericErrorMessageVisibleForTesting() override;
  std::string GetBubbleLabelTextForTesting() override;
  void OnLanguageIdentificationEvent(
      CaptionBubbleContext* caption_bubble_context,
      const media::mojom::LanguageIdentificationEventPtr& event) override;
  void CloseActiveModelForTesting() override;
  views::Widget* GetCaptionWidgetForTesting();
  CaptionBubble* GetCaptionBubbleForTesting();

 private:
  friend class CaptionBubbleControllerViewsTest;
  friend class LiveCaptionUnavailabilityNotifierTest;

  // SodaInstaller::Observer overrides:
  void OnSodaInstalled(speech::LanguageCode language_code) override;
  void OnSodaInstallError(speech::LanguageCode language_code,
                          speech::SodaInstaller::ErrorCode error_code) override;
  void OnSodaProgress(speech::LanguageCode language_code,
                      int progress) override;

  // A callback passed to the CaptionBubble which is called when the
  // CaptionBubble is destroyed.
  void OnCaptionBubbleDestroyed();

  // Sets the active CaptionBubbleModel to the one corresponding to the given
  // media player id, and creates a new CaptionBubbleModel if one does not
  // already exist.
  void SetActiveModel(CaptionBubbleContext* caption_bubble_context);

  // Called when the user closes the caption bubble.
  void OnSessionEnded(const std::string& session_id);

  // Called on a cross-origin navigation or reload.
  void OnSessionReset(const std::string& session_id);

  raw_ptr<CaptionBubble, DanglingUntriaged> caption_bubble_;
  raw_ptr<views::Widget, DanglingUntriaged> caption_widget_;

  // A pointer to the currently active CaptionBubbleModel.
  raw_ptr<CaptionBubbleModel, DanglingUntriaged> active_model_ = nullptr;

  // A map of media player ids and their corresponding CaptionBubbleModel. New
  // entries are added to this map when a previously unseen media player id is
  // received.
  std::unordered_map<CaptionBubbleContext*, std::unique_ptr<CaptionBubbleModel>>
      caption_bubble_models_;

  // A collection of closed session identifiers that should not display
  // captions. Identifiers are removed from this collection when a user
  // refreshes the page or navigates away.
  std::set<std::string> closed_sessions_;

  // Mapping of unique session identifiers to the observer that observes the
  // sessions.
  std::unordered_map<std::string, std::unique_ptr<CaptionBubbleSessionObserver>>
      caption_bubble_session_observers_;

  std::string application_locale_;

  base::WeakPtrFactory<CaptionBubbleControllerViews> weak_factory_{this};
};
}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_VIEWS_CAPTION_BUBBLE_CONTROLLER_VIEWS_H_
