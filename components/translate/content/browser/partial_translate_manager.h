// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CONTENT_BROWSER_PARTIAL_TRANSLATE_MANAGER_H_
#define COMPONENTS_TRANSLATE_CONTENT_BROWSER_PARTIAL_TRANSLATE_MANAGER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/contextual_search/core/browser/contextual_search_context.h"
#include "components/contextual_search/core/browser/contextual_search_delegate.h"
#include "components/contextual_search/core/browser/resolved_search_term.h"
#include "content/public/browser/web_contents.h"

// Structure used to pass context needed to resolve a partial translation.
struct PartialTranslateRequest {
  PartialTranslateRequest();
  PartialTranslateRequest(const PartialTranslateRequest& other);
  ~PartialTranslateRequest();

  // The selected text.
  std::u16string selection_text;

  // The selection's encoding.
  std::string selection_encoding;

  // The source language to translate from. If this isn't specified the server
  // will attempt to detect the selection language.
  std::optional<std::string> source_language;

  // The desired target language.
  std::string target_language;

  // Whether or not |source_language| should be applied as a hint for backend
  // language detection. Otherwise, backend translation is forced using
  // |source_language|.
  bool apply_lang_hint = false;
};

// Indicates the outcome of a Partial Translate request.
enum class PartialTranslateStatus {
  kSuccess,
  kError,
};

// Structure used to return translation details for a given partial translate.
struct PartialTranslateResponse {
  PartialTranslateResponse();
  PartialTranslateResponse(const PartialTranslateResponse& other);
  ~PartialTranslateResponse();

  // The result status.
  PartialTranslateStatus status;

  // The translated text.
  std::u16string translated_text;

  // The source language used for the translation. It may be different than what
  // was passed in the request.
  std::string source_language;

  // The target language used for the translation.
  std::string target_language;
};

// PartialTranslateManager handles translation of user-selected strings of text.
class PartialTranslateManager {
 public:
  typedef base::OnceCallback<void(const PartialTranslateResponse&)>
      PartialTranslateCallback;

  explicit PartialTranslateManager(
      std::unique_ptr<ContextualSearchDelegate> delegate);

  PartialTranslateManager(const PartialTranslateManager&) = delete;
  PartialTranslateManager& operator=(const PartialTranslateManager&) = delete;

  ~PartialTranslateManager();

  // Starts a partial translate request and cancels any ongoing request. Will
  // call |callback| once the request is completed (unless another request
  // subsumes it).
  void StartPartialTranslate(content::WebContents* web_contents,
                             const PartialTranslateRequest& request,
                             PartialTranslateCallback callback);

 private:
  // Creates a ContextualSearchContext given a PartialTranslateRequest.
  std::unique_ptr<ContextualSearchContext> MakeContext(
      const PartialTranslateRequest& request) const;

  // Creates a PartialTranslateResponse based on the response from the
  // ContextualSearchDelegate.
  PartialTranslateResponse MakeResponse(
      const ResolvedSearchTerm& resolved_search_term) const;

  // Callback called when the contextual search request finishes.
  void OnResolvedSearchTerm(const ResolvedSearchTerm& resolved_search_term);

  // The ContextualSearchContext generated for the current request (if any).
  // Owned here so we can create WeakPtrs for use with ContextualSearchDelegate.
  std::unique_ptr<ContextualSearchContext> context_;

  // The callback to call when the translation completes (successful or
  // otherwise).
  PartialTranslateCallback callback_;

  // The delegate we're using the do the real work.
  std::unique_ptr<ContextualSearchDelegate> delegate_;

  // Use a WeakPtrFactory so we can cancel in-flight Partial Translate requests.
  base::WeakPtrFactory<PartialTranslateManager> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_TRANSLATE_CONTENT_BROWSER_PARTIAL_TRANSLATE_MANAGER_H_
