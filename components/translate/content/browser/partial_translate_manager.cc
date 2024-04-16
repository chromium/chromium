// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/partial_translate_manager.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"

namespace {
const char kTranslatePartialTranslationHttpResponseCode[] =
    "Translate.PartialTranslation.HttpResponseCode";
}  // namespace

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
    const PartialTranslateRequest& request,
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
  context->SetBasePageUrl(GURL());

  context->SetSelectionSurroundings(0, request.selection_text.length(),
                                    request.selection_text);
  context->SetBasePageEncoding(request.selection_encoding);

  // The server won't translate text that's in one of the user's fluent
  // languages, so don't send fluent languages.
  context->SetTranslationLanguages(request.source_language.value_or(""),
                                   request.target_language,
                                   /*fluent_languages=*/"");
  context->SetApplyLangHint(request.apply_lang_hint);

  return context;
}

PartialTranslateResponse PartialTranslateManager::MakeResponse(
    const ResolvedSearchTerm& resolved_search_term) const {
  PartialTranslateResponse response;

  base::UmaHistogramSparse(kTranslatePartialTranslationHttpResponseCode,
                           resolved_search_term.response_code);

  if (resolved_search_term.response_code != 200) {
    response.status = PartialTranslateStatus::kError;
    return response;
  }
  response.status = PartialTranslateStatus::kSuccess;

  // The translated text is returned in the `caption` field.
  response.translated_text = base::UTF8ToUTF16(resolved_search_term.caption);
  response.source_language = resolved_search_term.context_language;

  // context_ may have been disposed of in the meantime.
  if (context_) {
    // TODO(crbug.com/40236584): Update this to pull from the
    // resolved_search_term once the server supports returning target language.
    response.target_language =
        context_->GetTranslationLanguages().target_language;
  }

  return response;
}

void PartialTranslateManager::OnResolvedSearchTerm(
    const ResolvedSearchTerm& resolved_search_term) {
  std::move(callback_).Run(MakeResponse(resolved_search_term));
}
