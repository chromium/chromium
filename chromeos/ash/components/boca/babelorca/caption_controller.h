// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_CAPTION_CONTROLLER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_CAPTION_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/native_theme/caption_style.h"
#include "ui/native_theme/native_theme_observer.h"

class PrefChangeRegistrar;
class PrefService;

namespace captions {
class CaptionBubbleContext;
class CaptionBubbleController;
}  // namespace captions

namespace media {
struct SpeechRecognitionResult;
}  // namespace media

namespace ash::babelorca {

class CaptionController : public ui::NativeThemeObserver {
 public:
  class Delegate {
   public:
    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate() = default;

    virtual std::unique_ptr<::captions::CaptionBubbleController>
    CreateCaptionBubbleController(PrefService* profile_prefs,
                                  const std::string& application_locale) = 0;

    virtual void AddCaptionStyleObserver(ui::NativeThemeObserver* observer) = 0;

    virtual void RemoveCaptionStyleObserver(
        ui::NativeThemeObserver* observer) = 0;

   protected:
    Delegate() = default;
  };

  CaptionController(
      std::unique_ptr<::captions::CaptionBubbleContext> caption_bubble_context,
      PrefService* profile_prefs,
      const std::string& application_locale,
      std::unique_ptr<Delegate> delegate = nullptr);

  CaptionController(const CaptionController&) = delete;
  CaptionController& operator=(const CaptionController&) = delete;

  ~CaptionController() override;

  // Routes a transcription to the CaptionBubbleController. Returns whether the
  // transcription result was routed successfully.
  bool DispatchTranscription(const media::SpeechRecognitionResult& result);

  // Alerts the CaptionBubbleController that the audio stream has ended.
  void OnAudioStreamEnd();

  void StartLiveCaption();

  void StopLiveCaption();

 private:
  // ui::NativeThemeObserver:
  void OnCaptionStyleUpdated() override;

  std::unique_ptr<::captions::CaptionBubbleContext> caption_bubble_context_;
  raw_ptr<PrefService> profile_prefs_;
  const std::string application_locale_;
  const std::unique_ptr<Delegate> delegate_;

  std::unique_ptr<::captions::CaptionBubbleController>
      caption_bubble_controller_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  std::optional<ui::CaptionStyle> caption_style_;

  bool is_ui_constructed_ = false;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_CAPTION_CONTROLLER_H_
