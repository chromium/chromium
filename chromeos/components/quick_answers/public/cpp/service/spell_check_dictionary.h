// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_SERVICE_SPELL_CHECK_DICTIONARY_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_SERVICE_SPELL_CHECK_DICTIONARY_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/components/quick_answers/public/mojom/spell_check.mojom.h"

class Hunspell;

namespace base {
class MemoryMappedFile;
}  // namespace base

namespace quick_answers {

// Utility class for spell check ran in renderer process.
class SpellCheckDictionary : public mojom::SpellCheckDictionary {
 public:
  SpellCheckDictionary();

  SpellCheckDictionary(const SpellCheckDictionary&) = delete;
  SpellCheckDictionary& operator=(const SpellCheckDictionary&) = delete;

  ~SpellCheckDictionary() override;

  bool Initialize(base::File file);

  void CheckSpelling(const std::string& word,
                     CheckSpellingCallback callback) override;

 private:
  std::unique_ptr<base::MemoryMappedFile> mapped_dict_file_;
  std::unique_ptr<Hunspell> hunspell_;
};

}  // namespace quick_answers

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_SERVICE_SPELL_CHECK_DICTIONARY_H_
