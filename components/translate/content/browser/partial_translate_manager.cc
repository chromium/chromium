// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/partial_translate_manager.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

PartialTranslateRequest::PartialTranslateRequest() = default;
PartialTranslateRequest::PartialTranslateRequest(
    const PartialTranslateRequest& other) = default;
PartialTranslateRequest::~PartialTranslateRequest() = default;

PartialTranslateResponse::PartialTranslateResponse() = default;
PartialTranslateResponse::PartialTranslateResponse(
    const PartialTranslateResponse& other) = default;
PartialTranslateResponse::~PartialTranslateResponse() = default;

PartialTranslateManager::PartialTranslateManager(
    std::unique_ptr<ContextualSearchDelegate> delegate)
    : delegate_(std::move(delegate)) {}

PartialTranslateManager::~PartialTranslateManager() = default;

void PartialTranslateManager::StartPartialTranslate(
    content::WebContents* web_contents,
    PartialTranslateRequest request,
    PartialTranslateCallback callback) {
  // Invalidate any ongoing request.
  weak_ptr_factory_.InvalidateWeakPtrs();

  context_ = MakeContext(request);
  callback_ = std::move(callback);
  delegate_->StartSearchTermResolutionRequest(
      context_->AsWeakPtr(), web_contents,
      base::BindRepeating(&PartialTranslateManager::OnResolvedSearchTerm,
                          weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<ContextualSearchContext> PartialTranslateManager::MakeContext(
    const PartialTranslateRequest& request) const {
  std::unique_ptr<ContextualSearchContext> context =
      std::make_unique<ContextualSearchContext>();

  context->SetRequestType(
      ContextualSearchContext::RequestType::PARTIAL_TRANSLATE);
  // Country and base URL are not needed for Partial Translate requests.
  context->SetResolveProperties(/*home_country=*/"",
                                /*may_send_base_page_url=*/false);
  context->SetBasePageUrl(GURL::EmptyGURL());

  context->SetSelectionSurroundings(0, request.selection_text.length(),
                                    request.selection_text);
  context->SetBasePageEncoding(request.selection_encoding);

  context->SetTranslationLanguages(
      request.source_language, request.target_language,
      base::JoinString(request.fluent_languages, ","));

  return context;
}

PartialTranslateResponse PartialTranslateManager::MakeResponse(
    const ResolvedSearchTerm& resolved_search_term) const {
  PartialTranslateResponse response;

  // The translated text is returned in the `caption` field.
  response.translated_text = base::UTF8ToUTF16(resolved_search_term.caption);
  response.source_language = resolved_search_term.context_language;

  // context_ may have been disposed of in the meantime.
  if (context_) {
    // TODO(crbug/1357202): Update this to pull from the resolved_search_term
    // once the server supports returning target language.
    response.target_language =
        context_->GetTranslationLanguages().target_language;
  }

  return response;
}

void PartialTranslateManager::OnResolvedSearchTerm(
    const ResolvedSearchTerm& resolved_search_term) {
  // TODO(crbug/1357202): Implement error handling, similar to that in Java at
  // https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/contextualsearch/ContextualSearchManager.java;drc=d36aa0494c1d0d5238eb4fd0e3c7db748d95fcbc;l=737.
  std::move(callback_).Run(MakeResponse(resolved_search_term));
}
