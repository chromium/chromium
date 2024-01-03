// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CORE_BROWSER_CONTEXTUAL_SEARCH_CONTEXT_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CORE_BROWSER_CONTEXTUAL_SEARCH_CONTEXT_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "url/gurl.h"

// Encapsulates key parts of a Contextual Search Context, including surrounding
// text.
class ContextualSearchContext {
 public:
  // Languages used for translation.
  struct TranslationLanguages {
    std::string detected_language;
    std::string target_language;
    std::string fluent_languages;
  };

  ContextualSearchContext();

  ContextualSearchContext(const ContextualSearchContext&) = delete;
  ContextualSearchContext& operator=(const ContextualSearchContext&) = delete;

  virtual ~ContextualSearchContext();

  // Returns whether this context can be resolved.
  // The context can be resolved only after calling SetResolveProperties.
  bool CanResolve() const { return can_resolve_; }

  // Returns whether the base page URL may be sent (according to policy).
  bool CanSendBasePageUrl() const { return can_send_base_page_url_; }

  // Sets the properties needed to resolve a context.
  void SetResolveProperties(const std::string& home_country,
                            bool may_send_base_page_url);

  // Adjust the current selection offsets by the given signed amounts.
  void AdjustSelection(int start_adjust, int end_adjust);

  // Gets the URL of the base page.
  const GURL& GetBasePageUrl() const { return base_page_url_; }
  // Sets the URL of the base page.
  void SetBasePageUrl(const GURL& base_page_url) {
    base_page_url_ = base_page_url;
  }

  // Gets the encoding of the base page.  This is not very important, since
  // the surrounding text stored here in a std::u16string is implicitly encoded
  // in UTF-16 (see http://www.chromium.org/developers/chromium-string-usage).
  const std::string& GetBasePageEncoding() const { return base_page_encoding_; }
  void SetBasePageEncoding(const std::string& base_page_encoding) {
    base_page_encoding_ = base_page_encoding;
  }

  // Gets the country code of the home country of the user, or an empty string.
  const std::string& GetHomeCountry() const { return home_country_; }

  // Sets the selection and surroundings.
  void SetSelectionSurroundings(int start_offset,
                                int end_offset,
                                const std::u16string& surrounding_text) {
    start_offset_ = start_offset;
    end_offset_ = end_offset;
    surrounding_text_ = surrounding_text;
  }

  // Gets the text surrounding the selection (including the selection).
  const std::u16string& GetSurroundingText() const { return surrounding_text_; }

  // Gets the start offset of the selection within the surrounding text (in
  // characters).
  int GetStartOffset() const { return start_offset_; }
  // Gets the end offset of the selection within the surrounding text (in
  // characters).
  int GetEndOffset() const { return end_offset_; }

  int64_t GetPreviousEventId() const { return previous_event_id_; }
  int GetPreviousEventResults() const { return previous_event_results_; }

  // Prepares the context to be used in a resolve request by supplying last
  // minute parameters.
  // |is_exact_resolve| indicates if the resolved term should be an exact match
  // for the selection range instead of an expandable selection.
  // |related_searches_stamp| is a value to stamp onto search URLs to identify
  // related searches. If the string is empty then Related Searches are not
  // being requested.
  void PrepareToResolve(bool is_exact_resolve,
                        const std::string& related_searches_stamp);

  // Returns whether the resolve request is for an exact match instead of an
  // expandable term.
  bool GetExactResolve() const { return is_exact_resolve_; }

  // Detects the language of the context using CLD from the translate utility.
  std::string DetectLanguage() const;

  // Sets the languages to remember for use in translation.
  // See |GetTranslationLanguages|.
  void SetTranslationLanguages(const std::string& detected_language,
                               const std::string& target_language,
                               const std::string& fluent_languages) {
    translation_languages_.detected_language = detected_language;
    translation_languages_.target_language = target_language;
    translation_languages_.fluent_languages = fluent_languages;
  }

  // Returns the languages to use for translation, as set by
  // |SetTranslationLanguages|.
  const TranslationLanguages& GetTranslationLanguages() const {
    return translation_languages_;
  }

  // Indicates the type of request that is being made to the Contextual Search
  // service.
  enum class RequestType {
    // A base contextual search request.
    CONTEXTUAL_SEARCH,
    // A contextual search request that also includes information around related
    // searches.
    RELATED_SEARCHES,
    // A request to translate a selection of text, used by the Partial Translate
    // functionality on desktop.
    PARTIAL_TRANSLATE,
  };

  // Gets the type of request that the context was collected for.
  RequestType GetRequestType() const { return request_type_; }
  void SetRequestType(RequestType request_type) {
    request_type_ = request_type;
  }

  // Get the logging information stamp for Related Searches requests or the
  // empty string if the feature is not enabled.
  const std::string& GetRelatedSearchesStamp() const {
    return related_searches_stamp_;
  }

  // Returns whether the source language of the context should be used as a hint
  // for backend language detection.
  bool GetApplyLangHint() const { return apply_lang_hint_; }
  void SetApplyLangHint(bool apply_lang_hint) {
    apply_lang_hint_ = apply_lang_hint;
  }

  virtual base::WeakPtr<ContextualSearchContext> AsWeakPtr();

 private:
  // Gets the reliable language of the given |contents| using CLD, or an empty
  // string if none can reliably be determined.
  std::string GetReliableLanguage(const std::u16string& contents) const;

  // Gets the selection, or an empty string if none.
  std::u16string GetSelection() const;

  bool can_resolve_ = false;
  RequestType request_type_ = RequestType::CONTEXTUAL_SEARCH;
  bool can_send_base_page_url_ = false;
  std::string home_country_;
  GURL base_page_url_;
  std::string base_page_encoding_;
  std::u16string surrounding_text_;
  int start_offset_ = 0;
  int end_offset_ = 0;
  int64_t previous_event_id_ = 0L;
  int previous_event_results_ = 0;
  bool is_exact_resolve_ = false;
  TranslationLanguages translation_languages_;
  std::string related_searches_stamp_;
  bool apply_lang_hint_ = false;

  base::WeakPtrFactory<ContextualSearchContext> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CORE_BROWSER_CONTEXTUAL_SEARCH_CONTEXT_H_
