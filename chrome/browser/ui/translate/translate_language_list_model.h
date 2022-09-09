// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_LANGUAGE_LIST_MODEL_H_
#define CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_LANGUAGE_LIST_MODEL_H_

#include <string>

// The model for the language lists used in the Full and Partial Translate
// bubble UXs.
class TranslateLanguageListModel {
 public:
  virtual ~TranslateLanguageListModel() = default;

  // Returns the number of source languages supported.
  virtual int GetNumberOfSourceLanguages() const = 0;

  // Returns the number of target languages supported.
  virtual int GetNumberOfTargetLanguages() const = 0;

  // Returns the displayable name for the source language at |index|.
  virtual std::u16string GetSourceLanguageNameAt(int index) const = 0;

  // Returns the displayable name for the target language at |index|.
  virtual std::u16string GetTargetLanguageNameAt(int index) const = 0;

  // Returns the source language index.
  virtual int GetSourceLanguageIndex() const = 0;

  // Updates the source language index.
  virtual void UpdateSourceLanguageIndex(int index) = 0;

  // Returns the target language index.
  virtual int GetTargetLanguageIndex() const = 0;

  // Updates the target language index.
  virtual void UpdateTargetLanguageIndex(int index) = 0;
};

#endif  // CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_LANGUAGE_LIST_MODEL_H_
