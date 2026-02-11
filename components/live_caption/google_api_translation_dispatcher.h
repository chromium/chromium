// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_GOOGLE_API_TRANSLATION_DISPATCHER_H_
#define COMPONENTS_LIVE_CAPTION_GOOGLE_API_TRANSLATION_DISPATCHER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/live_caption/translation_dispatcher.h"
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

class GoogleApiTranslationDispatcher : public TranslationDispatcher {
 public:
  GoogleApiTranslationDispatcher();
  ~GoogleApiTranslationDispatcher() override;
  GoogleApiTranslationDispatcher(const GoogleApiTranslationDispatcher&) =
      delete;
  GoogleApiTranslationDispatcher& operator=(
      const GoogleApiTranslationDispatcher&) = delete;
  GoogleApiTranslationDispatcher(std::string api_key,
                                 content::BrowserContext* browser_context);

  void GetTranslation(absl::string_view result,
                      absl::string_view source_language,
                      absl::string_view target_language,
                      TranslateEventCallback callback) override;

  void SetURLLoaderFactoryForTest(
      mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory);

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

  base::WeakPtrFactory<GoogleApiTranslationDispatcher> weak_factory_{this};
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_GOOGLE_API_TRANSLATION_DISPATCHER_H_
