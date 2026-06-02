// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_BROWSER_CONTEXTUAL_TRANSLATE_DELEGATE_H_
#define COMPONENTS_TRANSLATE_CONTENT_BROWSER_CONTEXTUAL_TRANSLATE_DELEGATE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/translate/content/browser/partial_translate_manager.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

class PrefService;

// ContextualTranslateDelegate handles translating user-selected text via
// the One Platform Google Translate API. It constructs and sends the requests,
// and parses the returning JSON responses.
class ContextualTranslateDelegate {
 public:
  using Callback = PartialTranslateManager::PartialTranslateCallback;

  explicit ContextualTranslateDelegate(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~ContextualTranslateDelegate();

  // Starts a partial translate request. If there is already an ongoing request,
  // it is immediately canceled.
  //
  // |request| contains the translation request details (selection, languages).
  // |prefs| is used to determine the API endpoint based on data region
  // settings.
  // |callback| will be called with the translation outcome (success or error).
  void StartPartialTranslate(const PartialTranslateRequest& request,
                             PrefService* prefs,
                             Callback callback);

 private:
  // Callback invoked when the network request to the Translate API completes.
  // It checks the response code and, on success, starts parsing the JSON body.
  void OnUrlLoadComplete(std::string target_language,
                         std::optional<std::string> source_language,
                         std::optional<std::string> response_body);

  // Callback invoked when the JSON response from the Translate API is parsed.
  // It extracts the translated text and source language, then runs the
  // pending callback with the result.
  void OnJsonParsed(std::string target_language,
                    std::optional<std::string> source_language,
                    data_decoder::DataDecoder::ValueOrError result);

  // Cancels any ongoing request and fires the callback with an error status.
  // Returns true if the object is still alive after the callback is run, or
  // false if it was destroyed.
  bool CancelPendingRequest();

  // Returns the appropriate Translate API endpoint URL based on the user's
  // data region preference.
  GURL BuildEndpointUrl(PrefService* prefs) const;

  // Formats the translation request details into a JSON payload string
  // expected by the Translate API endpoint.
  std::string CreatePostData(const PartialTranslateRequest& request) const;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  Callback pending_callback_;
  data_decoder::DataDecoder data_decoder_;
  base::WeakPtrFactory<ContextualTranslateDelegate> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_TRANSLATE_CONTENT_BROWSER_CONTEXTUAL_TRANSLATE_DELEGATE_H_
