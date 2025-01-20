// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_TRANSLATION_DISPATCHER_H_
#define COMPONENTS_LIVE_CAPTION_TRANSLATION_DISPATCHER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/live_caption/translation_util.h"
#include "components/soda/constants.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace captions {

///////////////////////////////////////////////////////////////////////////////
// Live Translate Dispatcher
//
//  Sends out the request to Cloud Translate API. The owning class provides the
//  API key which is used to call the service.
//
class TranslationDispatcher {
 public:
  TranslationDispatcher(std::string api_key,
                        content::BrowserContext* browser_context);
  TranslationDispatcher(const TranslationDispatcher&) = delete;
  TranslationDispatcher& operator=(const TranslationDispatcher&) = delete;
  ~TranslationDispatcher();

  void GetTranslation(const std::string& result,
                      std::string source_language,
                      std::string target_language,
                      TranslateEventCallback callback);

 private:
  void ResetURLLoaderFactory();
  void OnURLLoadComplete(TranslateEventCallback callback,
                         std::optional<std::string> response_body);

  void EmitError(TranslateEventCallback callback,
                 const std::string& string) const;

  // Called when the data decoder service provides parsed JSON data for a server
  // response.
  void OnResponseJsonParsed(TranslateEventCallback callback,
                            data_decoder::DataDecoder::ValueOrError result);

  const std::string api_key_;
  raw_ptr<content::BrowserContext> browser_context_;

  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  data_decoder::DataDecoder data_decoder_;

  base::WeakPtrFactory<TranslationDispatcher> weak_factory_{this};
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_TRANSLATION_DISPATCHER_H_
