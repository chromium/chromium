// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_LIVE_TRANSLATE_CONTROLLER_H_
#define COMPONENTS_LIVE_CAPTION_LIVE_TRANSLATE_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/live_caption/translation_dispatcher.h"
#include "components/live_caption/translation_util.h"
#include "media/mojo/mojom/speech_recognition_result.h"

class PrefChangeRegistrar;
class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace captions {

///////////////////////////////////////////////////////////////////////////////
// Live Translate Controller
//
//  The controller of the live translate feature. The live translate controller
//  is a KeyedService. There exists one live translate controller per profile
//  and it lasts for the duration of the sessions.
//
class LiveTranslateController : public KeyedService {
 public:
  LiveTranslateController(
      PrefService* profile_prefs,
      std::unique_ptr<TranslationDispatcher> translation_dispatcher,
      std::unique_ptr<TranslationDispatcher> google_api_dispatcher);
  LiveTranslateController(const LiveTranslateController&) = delete;
  LiveTranslateController& operator=(const LiveTranslateController&) = delete;
  ~LiveTranslateController() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  virtual void GetTranslation(const std::string& result,
                              std::string source_language,
                              std::string target_language,
                              TranslateEventCallback callback);

 private:
  void OnLiveTranslateEnabledChanged();
  void OnOnDeviceTranslated(std::string_view result,
                            std::string_view source_language,
                            std::string_view target_language,
                            TranslateEventCallback callback,
                            base::TimeTicks start_time,
                            const TranslateEvent& translate_event);
  void OnGoogleApiTranslated(TranslateEventCallback callback,
                             base::TimeTicks total_start_time,
                             base::TimeTicks google_api_start_time,
                             const TranslateEvent& translate_event);
  raw_ptr<PrefService> profile_prefs_;
  const std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  const std::unique_ptr<TranslationDispatcher> on_device_dispatcher_;
  const std::unique_ptr<TranslationDispatcher> google_api_dispatcher_;
  base::WeakPtrFactory<LiveTranslateController> weak_factory_{this};
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_LIVE_TRANSLATE_CONTROLLER_H_
