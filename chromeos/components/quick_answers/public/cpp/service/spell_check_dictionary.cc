// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/public/cpp/service/spell_check_dictionary.h"

#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "third_party/hunspell/src/hunspell/hunspell.hxx"

namespace quick_answers {

SpellCheckDictionary::SpellCheckDictionary() = default;

SpellCheckDictionary::~SpellCheckDictionary() = default;

bool SpellCheckDictionary::Initialize(base::File file) {
  mapped_dict_file_ = std::make_unique<base::MemoryMappedFile>();

  if (!mapped_dict_file_->Initialize(std::move(file))) {
    LOG(ERROR) << "Failed to mmap dictionary file.";
    return false;
  }

  if (!hunspell::BDict::Verify(mapped_dict_file_->bytes())) {
    LOG(ERROR) << "Failed to verify dictionary file.";
    return false;
  }

  hunspell_ = std::make_unique<Hunspell>(mapped_dict_file_->bytes());

  return true;
}

void SpellCheckDictionary::CheckSpelling(const std::string& word,
                                         CheckSpellingCallback callback) {
  DCHECK(hunspell_);

  std::move(callback).Run(hunspell_->spell(word) != 0);
}

}  // namespace quick_answers
