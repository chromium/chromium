// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TRANSLATE_PARTIAL_TRANSLATE_BUBBLE_MODEL_IMPL_H_
#define CHROME_BROWSER_UI_TRANSLATE_PARTIAL_TRANSLATE_BUBBLE_MODEL_IMPL_H_

#include "base/observer_list.h"
#include "chrome/browser/ui/translate/partial_translate_bubble_model.h"

class PartialTranslateManager;
struct PartialTranslateRequest;
struct PartialTranslateResponse;

namespace translate {
class TranslateUILanguagesManager;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PartialTranslateTranslationStatus {
  kSuccess = 0,
  kError = 1,
  kMaxValue = kError,
};

class PartialTranslateBubbleModelImpl : public PartialTranslateBubbleModel {
 public:
  PartialTranslateBubbleModelImpl(
      ViewState view_state,
      translate::TranslateErrors error_type,
      const std::u16string& source_text,
      const std::u16string& target_text,
      std::unique_ptr<PartialTranslateManager> partial_translate_manager,
      std::unique_ptr<translate::TranslateUILanguagesManager>
          translate_ui_languages_manager);

  PartialTranslateBubbleModelImpl(const PartialTranslateBubbleModelImpl&) =
      delete;
  PartialTranslateBubbleModelImpl& operator=(
      const PartialTranslateBubbleModelImpl&) = delete;

  ~PartialTranslateBubbleModelImpl() override;

  // PartialTranslateBubbleModel methods:
  void AddObserver(PartialTranslateBubbleModel::Observer* obs) override;
  void RemoveObserver(PartialTranslateBubbleModel::Observer* obs) override;
  ViewState GetViewState() const override;
  void SetViewState(ViewState view_state) override;
  void SetSourceLanguage(const std::string& language_code) override;
  void SetTargetLanguage(const std::string& language_code) override;
  void SetSourceText(const std::u16string& text) override;
  std::u16string GetSourceText() const override;
  void SetTargetText(const std::u16string& text) override;
  std::u16string GetTargetText() const override;
  void SetError(translate::TranslateErrors error_type) override;
  translate::TranslateErrors GetError() const override;
  int GetNumberOfSourceLanguages() const override;
  int GetNumberOfTargetLanguages() const override;
  std::u16string GetSourceLanguageNameAt(int index) const override;
  std::u16string GetTargetLanguageNameAt(int index) const override;
  int GetSourceLanguageIndex() const override;
  void UpdateSourceLanguageIndex(int index) override;
  int GetTargetLanguageIndex() const override;
  void UpdateTargetLanguageIndex(int index) override;
  std::string GetSourceLanguageCode() const override;
  std::string GetTargetLanguageCode() const override;
  void Translate(content::WebContents* web_contents) override;
  void TranslateFullPage(content::WebContents* web_contents) override;
  void SetSourceTextTruncated(bool is_truncated) override;

 private:
  // Updates the partial translate model based on the given response.
  void OnPartialTranslateResponse(const PartialTranslateRequest& request,
                                  const PartialTranslateResponse& response);

  // Logs relevant information about a partial translation when it is initiated.
  void RecordHistogramsOnPartialTranslateStart();

  // Logs relevant information about a partial translation when it is completed.
  void RecordHistogramsOnPartialTranslateComplete(bool status_error);

  // The current view type.
  ViewState current_view_state_;

  // The current error, or NONE if there is none.
  translate::TranslateErrors error_type_;

  // The selected text, which may be truncated.
  std::u16string source_text_;

  // Whether or not the current selected text is truncated.
  bool source_text_truncated_;

  // The translated text, or empty if the translation has not yet been
  // performed.
  std::u16string target_text_;

  // Whether or not the model has completed a successful Partial Translate
  // request. This is always false on initialization.
  bool initial_request_completed_ = false;

  // Time when the PartialTranslateManager is directed to start a translation.
  // This is used to know the response time of a Partial Translate request.
  base::TimeTicks translate_request_started_time_;

  // Time when the Partial Translate response is received. This is used to know
  // the response time of a Partial Translate request.
  base::TimeTicks translate_response_received_time_;

  // A manager instance to handle translation of user selected strings.
  std::unique_ptr<PartialTranslateManager> partial_translate_manager_;

  // Used to track the source and target languages.
  std::unique_ptr<translate::TranslateUILanguagesManager> ui_languages_manager_;

  // A list of clients to notify of partial translate status changes.
  base::ObserverList<PartialTranslateBubbleModel::Observer> observers_;
};

#endif  // CHROME_BROWSER_UI_TRANSLATE_PARTIAL_TRANSLATE_BUBBLE_MODEL_IMPL_H_
