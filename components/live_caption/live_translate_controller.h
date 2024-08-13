// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_LIVE_TRANSLATE_CONTROLLER_H_
#define COMPONENTS_LIVE_CAPTION_LIVE_TRANSLATE_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/soda/constants.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

class PrefChangeRegistrar;
class PrefService;

namespace content {
class BrowserContext;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace network {
class SimpleURLLoader;
}  // namespace network

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
  void ResetURLLoaderFactory();
  void OnURLLoadComplete(OnTranslateEventCallback callback,
                         std::unique_ptr<std::string> response_body);

  // Called when the data decoder service provides parsed JSON data for a server
  // response.
  void OnResponseJsonParsed(OnTranslateEventCallback callback,
                            data_decoder::DataDecoder::ValueOrError result);

  void OnLiveTranslateEnabledChanged();

  raw_ptr<content::BrowserContext> browser_context_;
  raw_ptr<PrefService> profile_prefs_;

  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  data_decoder::DataDecoder data_decoder_;

  const std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  base::WeakPtrFactory<LiveTranslateController> weak_factory_{this};
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_LIVE_TRANSLATE_CONTROLLER_H_
