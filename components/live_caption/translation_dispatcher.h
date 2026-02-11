// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_TRANSLATION_DISPATCHER_H_
#define COMPONENTS_LIVE_CAPTION_TRANSLATION_DISPATCHER_H_

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/live_caption/translation_util.h"
#include "components/soda/constants.h"
#include "media/mojo/mojom/speech_recognition_result.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace captions {

///////////////////////////////////////////////////////////////////////////////
// Live Translate Dispatcher
//
//  Sends out the request to Cloud Translate API. The owning class provides the
//  API key which is used to call the service.
//
class TranslationDispatcher {
 public:
  TranslationDispatcher() = default;
  TranslationDispatcher(const TranslationDispatcher&) = delete;
  TranslationDispatcher& operator=(const TranslationDispatcher&) = delete;
  virtual ~TranslationDispatcher() = default;

  virtual void GetTranslation(absl::string_view result,
                              absl::string_view source_language,
                              absl::string_view target_language,
                              TranslateEventCallback callback) = 0;
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_TRANSLATION_DISPATCHER_H_
