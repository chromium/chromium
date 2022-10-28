// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_LIVE_TRANSLATE_CONTROLLER_H_
#define COMPONENTS_LIVE_CAPTION_LIVE_TRANSLATE_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/soda/constants.h"

class PrefChangeRegistrar;
class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace captions {

///////////////////////////////////////////////////////////////////////////////
// Live Translate Controller
//
//  The controller of the live translate feature. The live translate controller
//  is a KeyedService. There exists one live translate controller per profile
//  and it lasts for the duration of the session.
//
class LiveTranslateController : public KeyedService {
 public:
  explicit LiveTranslateController(PrefService* profile_prefs);
  LiveTranslateController(const LiveTranslateController&) = delete;
  LiveTranslateController& operator=(const LiveTranslateController&) = delete;
  ~LiveTranslateController() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  void OnLiveCaptionEnabledChanged();
  void OnLiveTranslateEnabledChanged();

  raw_ptr<PrefService> profile_prefs_;
  const std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_LIVE_TRANSLATE_CONTROLLER_H_
