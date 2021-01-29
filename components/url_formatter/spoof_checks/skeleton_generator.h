// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_SKELETON_GENERATOR_H_
#define COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_SKELETON_GENERATOR_H_

#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/strings/string_piece_forward.h"

#include "third_party/icu/source/common/unicode/uniset.h"

// 'icu' does not work. Use U_ICU_NAMESPACE.
namespace U_ICU_NAMESPACE {

class Transliterator;

}  // namespace U_ICU_NAMESPACE

struct USpoofChecker;

using Skeletons = base::flat_set<std::string>;

// This class generates skeleton strings from hostnames. Skeletons are a
// transformation of the input string. Two hostnames are confusable if their
// skeletons are identical. See http://unicode.org/reports/tr39/ for more
// information.
// This class uses ICU to generate skeletons. Before passing the input to ICU,
// it performs additional transformations (diacritic removal and extra
// confusable mapping of certain characters) so that more confusable hostnames
// can be detected than would be by using plain ICU API.
class SkeletonGenerator {
 public:
  explicit SkeletonGenerator(const USpoofChecker* checker);
  ~SkeletonGenerator();

  // Returns the set of skeletons for the |hostname|. For IDN, |hostname| must
  // already be decoded to unicode.
  Skeletons GetSkeletons(base::StringPiece16 hostname);

 private:
  // Adds an additional mapping from |src_char| to |mapped_char| when generating
  // skeletons: If |host| contains |src_char|, |skeletons| will contain a new
  // skeleton where all occurances of |src_char| are replaced with
  // |mapped_char|.
  void AddSkeletonMapping(const icu::UnicodeString& host,
                          int32_t src_char,
                          int32_t mapped_char,
                          Skeletons* skeletons);

  icu::UnicodeSet lgc_letters_n_ascii_;

  std::unique_ptr<icu::Transliterator> diacritic_remover_;
  std::unique_ptr<icu::Transliterator> extra_confusable_mapper_;

  const USpoofChecker* checker_;
};

#endif  // COMPONENTS_URL_FORMATTER_SPOOF_CHECKS_SKELETON_GENERATOR_H_
