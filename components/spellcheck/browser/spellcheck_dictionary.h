// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECK_DICTIONARY_H_
#define COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECK_DICTIONARY_H_

// Defines a dictionary for use in the spellchecker system and provides access
// to words within the dictionary.
class SpellcheckDictionary {
 public:
  SpellcheckDictionary() {}

  SpellcheckDictionary(const SpellcheckDictionary&) = delete;
  SpellcheckDictionary& operator=(const SpellcheckDictionary&) = delete;

  virtual ~SpellcheckDictionary() {}

  virtual void Load() = 0;

 protected:
};

#endif  // COMPONENTS_SPELLCHECK_BROWSER_SPELLCHECK_DICTIONARY_H_
