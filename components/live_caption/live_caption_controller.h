// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_LIVE_CAPTION_CONTROLLER_H_
#define COMPONENTS_LIVE_CAPTION_LIVE_CAPTION_CONTROLLER_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/live_caption/caption_controller_base.h"
#include "components/live_caption/views/caption_bubble.h"
#include "components/soda/constants.h"
#include "components/soda/soda_installer.h"
#include "ui/native_theme/caption_style.h"
#include "ui/native_theme/native_theme_observer.h"

namespace content {
class BrowserContext;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace captions {

class CaptionBubbleController;
class CaptionBubbleContext;
class LiveCaptionBubbleSettings;

///////////////////////////////////////////////////////////////////////////////
// Live Caption Controller
//
//  The controller of the live caption feature. It enables the captioning
//  service when the preference is enabled. The live caption controller is a
//  KeyedService. There exists one live caption controller per profile and it
//  lasts for the duration of the session. The live caption controller owns the
//  live caption UI, which is a caption bubble controller.
//
class LiveCaptionController : public KeyedService,
                              public speech::SodaInstaller::Observer,
                              public CaptionControllerBase {
 public:
  LiveCaptionController(
      PrefService* profile_prefs,
      PrefService* global_prefs,
      const std::string& application_locale,
      content::BrowserContext* browser_context,
      std::unique_ptr<CaptionControllerBase::Delegate> delegate = nullptr);
  ~LiveCaptionController() override;
  LiveCaptionController(const LiveCaptionController&) = delete;
  LiveCaptionController& operator=(const LiveCaptionController&) = delete;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Alerts the CaptionBubbleController that there is an error in the speech
  // recognition service.
  void OnError(CaptionBubbleContext* caption_bubble_context,
               CaptionBubbleErrorType error_type,
               OnErrorClickedCallback error_clicked_callback,
               OnDoNotShowAgainClickedCallback error_silenced_callback);

  // Mac and ChromeOS move the fullscreened window into a new workspace. When
  // the WebContents associated with the CaptionBubbleContext goes
  // fullscreen, ensure that the Live Caption bubble moves to the new workspace.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  void OnToggleFullscreen(CaptionBubbleContext* caption_bubble_context);
#endif

  CaptionBubbleController* caption_bubble_controller_for_testing() {
    return caption_bubble_controller();
  }

 private:
  // CaptionControllerBase:
  CaptionBubbleSettings* caption_bubble_settings() override;

  // SodaInstaller::Observer:
  void OnSodaInstalled(speech::LanguageCode language_code) override;
  void OnSodaProgress(speech::LanguageCode language_code,
                      int progress) override {}
  void OnSodaInstallError(speech::LanguageCode language_code,
                          speech::SodaInstaller::ErrorCode error_code) override;

  void OnLiveCaptionEnabledChanged();
  void OnLiveCaptionLanguageChanged();
  bool IsLiveCaptionEnabled();
  void StartLiveCaption();
  void StopLiveCaption();
  const std::string GetLanguageCode() const;

  void MaybeSetLiveCaptionLanguage();

  raw_ptr<PrefService> global_prefs_ = nullptr;
  raw_ptr<content::BrowserContext> browser_context_ = nullptr;
  const std::unique_ptr<LiveCaptionBubbleSettings> caption_bubble_settings_;

  // Whether Live Caption is enabled.
  bool enabled_ = false;
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_LIVE_CAPTION_CONTROLLER_H_
