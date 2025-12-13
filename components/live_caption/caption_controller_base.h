// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_CAPTION_CONTROLLER_BASE_H_
#define COMPONENTS_LIVE_CAPTION_CAPTION_CONTROLLER_BASE_H_

#include <list>
#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "ui/native_theme/caption_style.h"
#include "ui/native_theme/native_theme_observer.h"

class PrefChangeRegistrar;
class PrefService;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace captions {

class CaptionBubbleContext;
class CaptionBubbleController;
class CaptionBubbleSettings;
class TranslationViewWrapperBase;

class CaptionControllerBase : public ui::NativeThemeObserver {
 public:
  class Delegate {
   public:
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate() = default;

    virtual std::unique_ptr<CaptionBubbleController>
    CreateCaptionBubbleController(
        CaptionBubbleSettings* caption_bubble_settings,
        const std::string& application_locale,
        std::unique_ptr<TranslationViewWrapperBase>
            translation_view_wrapper) = 0;

    virtual void AddCaptionStyleObserver(ui::NativeThemeObserver* observer) = 0;

    virtual void RemoveCaptionStyleObserver(
        ui::NativeThemeObserver* observer) = 0;

   protected:
    Delegate() = default;
  };

  // Listener for transcription-related events.  Listeners are owned by the
  // `CaptionControllerBase` for simplicity.
  class Listener {
   public:
    virtual ~Listener() = default;

    // Called when a transcription is received from the service for audio that
    // originated in the RFH.  This may be null if the audio was not associated
    // with any particulat RFH.
    //
    // Transcriptions will halt if this returns false.
    virtual bool OnTranscription(
        content::RenderFrameHost*,
        CaptionBubbleContext*,
        const media::SpeechRecognitionResult& result) = 0;

    // Called when the audio stream has ended for audio from the
    // RenderFrameHost, which may be null.
    virtual void OnAudioStreamEnd(
        content::RenderFrameHost*,
        CaptionBubbleContext* caption_bubble_context) = 0;

    // Called when the language is identified for audio from the
    // RenderFrameHost, which may be null.
    virtual void OnLanguageIdentificationEvent(
        content::RenderFrameHost*,
        CaptionBubbleContext* caption_bubble_context,
        const media::mojom::LanguageIdentificationEventPtr& event) = 0;

   private:
    raw_ptr<CaptionControllerBase> caption_controller_ = nullptr;
  };

  CaptionControllerBase(const CaptionControllerBase&) = delete;
  CaptionControllerBase& operator=(const CaptionControllerBase&) = delete;

  ~CaptionControllerBase() override;

  // Add or remove a `Listener`.  We maintain ownership, and destroy the
  // listener when it is removed.
  void AddListener(std::unique_ptr<Listener>);

  // Routes a transcription to all listeners.  Returns whether the transcription
  // was routed successfully, which currently means that at least one listener
  // considered it to be successful.  It is unclear if we should continue to
  // route transcriptions to a listener once it returns false, but for now
  // there's at most one listener anyway.
  //
  // Transcriptions will halt if this returns false.
  bool DispatchTranscription(content::RenderFrameHost* rfh,
                             CaptionBubbleContext* caption_bubble_context,
                             const media::SpeechRecognitionResult& result);

  // Alerts all listeners that the audio stream has ended.
  void OnAudioStreamEnd(content::RenderFrameHost* rfh,
                        CaptionBubbleContext* caption_bubble_context);

  // Notifies all listeners about a language identification event.
  void OnLanguageIdentificationEvent(
      content::RenderFrameHost* rfh,
      CaptionBubbleContext* caption_bubble_context,
      const media::mojom::LanguageIdentificationEventPtr& event);

  void create_ui_for_testing() { CreateUI(); }
  void destroy_ui_for_testing() { DestroyUI(); }
  CaptionBubbleController* caption_bubble_controller_for_testing() const {
    return caption_bubble_controller();
  }

  void remove_listener_for_testing(Listener* listener) {
    RemoveListener(listener);
  }

 protected:
  CaptionControllerBase(PrefService* profile_prefs,
                        const std::string& application_locale,
                        std::unique_ptr<Delegate> delegate = nullptr);

  void CreateUI();
  void DestroyUI();

  PrefService* profile_prefs() const;
  const std::string& application_locale() const;
  PrefChangeRegistrar* pref_change_registrar() const;
  CaptionBubbleController* caption_bubble_controller() const;

  virtual std::unique_ptr<TranslationViewWrapperBase>
  CreateTranslationViewWrapper();

  // Called when the size of the listener set goes to or from zero.  This allows
  // subclasses to handle SODA installation as needed on a per-platform basis.
  virtual void OnFirstListenerAdded() {}
  virtual void OnLastListenerRemoved() {}

 private:
  virtual CaptionBubbleSettings* caption_bubble_settings() = 0;

  // ui::NativeThemeObserver:
  void OnCaptionStyleUpdated() override;

  // The listener will be destroyed when this returns.  Private, so that we
  // don't have to worry about listeners removing themselves during list
  // iteration.  If Listeners ever want to do that, then we need to get smarter
  // about it.
  void RemoveListener(Listener*);

  const raw_ptr<PrefService> profile_prefs_ = nullptr;
  const std::string application_locale_;
  const std::unique_ptr<Delegate> delegate_;

  const std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Whether the UI has been created. The UI is created asynchronously from the
  // feature being enabled--some implementations may wait for SODA to download
  // first. This flag ensures that the UI is not constructed or deconstructed
  // twice.
  bool is_ui_constructed_ = false;

  // All the listeners we own.  One of them will be `caption_bubble_controller_`
  // if we have one.
  std::list<std::unique_ptr<Listener>> listeners_;

  // The controller is also a `Listener`, and is owned by `listeners_` when we
  // create it.  While it exists, `caption_bubble_controller_` aliases it.  This
  // alias is cleared when it's destroyed.
  raw_ptr<CaptionBubbleController> caption_bubble_controller_ = nullptr;
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_CAPTION_CONTROLLER_BASE_H_
