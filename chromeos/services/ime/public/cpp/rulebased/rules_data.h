// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_RULES_DATA_H_
#define CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_RULES_DATA_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"

namespace re2 {
class RE2;
}  // namespace re2

namespace chromeos {
namespace ime {
namespace rulebased {

using KeyMap = std::map<std::string, const char*>;

using TransformRule = std::pair<std::unique_ptr<re2::RE2>, std::string>;

class RulesData {
 public:
  RulesData();
  ~RulesData();

  // Creates the RulesData by the given raw data.
  static std::unique_ptr<RulesData> Create(const char*** key_map,
                                           bool is_102_keyboard,
                                           const char** transforms,
                                           const uint16_t transforms_count,
                                           const char* history_prune);

  // Finds the raw data entry with the given |id|, and use the raw data to
  // create a RulesData instance.
  static std::unique_ptr<RulesData> GetById(const std::string& id);

  static bool IsIdSupported(const std::string& id);

  // The caller does NOT own the returned key map instance.
  const KeyMap* GetKeyMapByModifiers(uint8_t modifiers) const;

  // Transform the input text, including the context and the appended text, with
  // the transform rules defined in this RulesData instance.
  // The caller must calculate the |transat| value which represents the position
  // where the previous transform occured.
  // e.g. the rule definition is "dd" -> "D", and user types "abcdde".
  // When user types "e" after the text "abcD", the caller must call this method
  // with context="abcD", transat=4, appended="e".
  bool Transform(const std::string& context,
                 int transat,
                 const std::string& appended,
                 std::string* transformed) const;

  bool HasTransform() const { return transform_re_merged_.get(); }

  // Predicts whether there will be future transform matches with the given
  // states of the context string and transat pos.
  bool PredictTransform(const std::string& str, int transat) const;

  bool MatchHistoryPrune(const std::string& str) const;

 private:
  // The KeyMap instances under all the modifier states.
  static constexpr size_t kKeyMapCount = 8;
  KeyMap key_maps_[kKeyMapCount];

  // The map from the sub group match index (of the merged regexp) to the
  // transform rule (which is a pair of matching regexp, and replace string).
  std::map<uint16_t, TransformRule> transform_rules_;

  // The merged regexp to do the quick check of whether certain text can match
  // one of the defined transform rules.
  std::unique_ptr<re2::RE2> transform_re_merged_;

  // The regexp used to check whether there would be future transform matches.
  std::unique_ptr<re2::RE2> prefix_re_;

  // The history prune regexp which is only used by client code of RulesData.
  std::unique_ptr<re2::RE2> history_prune_re_;

  DISALLOW_COPY_AND_ASSIGN(RulesData);
};

}  // namespace rulebased
}  // namespace ime
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_IME_PUBLIC_CPP_RULEBASED_RULES_DATA_H_
