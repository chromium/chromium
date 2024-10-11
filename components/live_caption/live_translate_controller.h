// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_LIVE_TRANSLATE_CONTROLLER_H_
#define COMPONENTS_LIVE_CAPTION_LIVE_TRANSLATE_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/live_caption/translation_dispatcher.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "mojo/public/cpp/bindings/remote.h"

class PrefChangeRegistrar;
class PrefService;

namespace content {
class BrowserContext;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace captions {

using OnTranslateEventCallback = base::OnceCallback<void(const std::string&)>;

///////////////////////////////////////////////////////////////////////////////
// Live Translate Controller
//
//  The controller of the live translate feature. The live translate controller
//  is a KeyedService. There exists one live translate controller per profile
//  and it lasts for the duration of the sessions.
//
class LiveTranslateController : public KeyedService {
 public:
  LiveTranslateController(PrefService* profile_prefs,
                          content::BrowserContext* browser_context);
  LiveTranslateController(const LiveTranslateController&) = delete;
  LiveTranslateController& operator=(const LiveTranslateController&) = delete;
  ~LiveTranslateController() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  virtual void GetTranslation(const std::string& result,
                              std::string source_language,
                              std::string target_language,
                              OnTranslateEventCallback callback);

 private:
  void OnLiveTranslateEnabledChanged();
  raw_ptr<PrefService> profile_prefs_;
  const std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  const std::unique_ptr<TranslationDispatcher> translation_dispatcher_;
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_LIVE_TRANSLATE_CONTROLLER_H_
