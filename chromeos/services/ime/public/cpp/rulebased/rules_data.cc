// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/public/cpp/rulebased/rules_data.h"

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ar.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/bn_phone.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ckb_ar.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ckb_en.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/deva_phone.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ethi.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/fa.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/gu_phone.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/km.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/kn_phone.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/lo.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ml_phone.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/my.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/my_myansan.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ne_inscript.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ne_phone.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ru_phone_aatseel.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ru_phone_yazhert.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/si.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ta_inscript.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ta_itrans.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ta_phone.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ta_tamil99.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/ta_typewriter.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/te_phone.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/th.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/th_pattajoti.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/th_tis.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/us.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/vi_tcvn.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/vi_telex.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/vi_viqr.h"
#include "chromeos/services/ime/public/cpp/rulebased/def/vi_vni.h"
#include "third_party/re2/src/re2/re2.h"

namespace chromeos {
namespace ime {
namespace rulebased {

namespace {

struct RawDataEntry {
  const char*** key_map;
  bool is_102_keyboard;
  const char** transforms;
  const uint16_t transforms_count;
  const char* history_prune;

  RawDataEntry(const char*** map, bool is_102)
      : RawDataEntry(map, is_102, nullptr, 0, nullptr) {}

  RawDataEntry(const char*** map,
               bool is_102,
               const char** trans,
               uint16_t trans_count,
               const char* prune)
      : key_map(map),
        is_102_keyboard(is_102),
        transforms(trans),
        transforms_count(trans_count),
        history_prune(prune) {}
};

const std::map<std::string, RawDataEntry>& GetRawData() {
  static const std::map<std::string, RawDataEntry> kRawData = {
      {ar::kId, RawDataEntry(ar::kKeyMap, ar::kIs102)},
      {bn_phone::kId,
       RawDataEntry(bn_phone::kKeyMap, bn_phone::kIs102, bn_phone::kTransforms,
                    bn_phone::kTransformsLen, bn_phone::kHistoryPrune)},
      {ckb_ar::kId, RawDataEntry(ckb_ar::kKeyMap, ckb_ar::kIs102)},
      {ckb_en::kId, RawDataEntry(ckb_en::kKeyMap, ckb_en::kIs102)},
      {deva_phone::kId,
       RawDataEntry(us::kKeyMap, us::kIs102, deva_phone::kTransforms,
                    deva_phone::kTransformsLen, deva_phone::kHistoryPrune)},
      {ethi::kId, RawDataEntry(ethi::kKeyMap, ethi::kIs102, ethi::kTransforms,
                               ethi::kTransformsLen, ethi::kHistoryPrune)},
      {fa::kId, RawDataEntry(fa::kKeyMap, fa::kIs102)},
      {gu_phone::kId,
       RawDataEntry(gu_phone::kKeyMap, gu_phone::kIs102, gu_phone::kTransforms,
                    gu_phone::kTransformsLen, gu_phone::kHistoryPrune)},
      {km::kId, RawDataEntry(km::kKeyMap, km::kIs102)},
      {kn_phone::kId,
       RawDataEntry(us::kKeyMap, us::kIs102, kn_phone::kTransforms,
                    kn_phone::kTransformsLen, kn_phone::kHistoryPrune)},
      {lo::kId, RawDataEntry(lo::kKeyMap, lo::kIs102)},
      {ml_phone::kId,
       RawDataEntry(us::kKeyMap, us::kIs102, ml_phone::kTransforms,
                    ml_phone::kTransformsLen, ml_phone::kHistoryPrune)},
      {my::kId, RawDataEntry(my::kKeyMap, my::kIs102, my::kTransforms,
                             my::kTransformsLen, my::kHistoryPrune)},
      {my_myansan::kId,
       RawDataEntry(my_myansan::kKeyMap, my_myansan::kIs102,
                    my_myansan::kTransforms, my_myansan::kTransformsLen,
                    my_myansan::kHistoryPrune)},
      {ne_inscript::kId,
       RawDataEntry(ne_inscript::kKeyMap, ne_inscript::kIs102)},
      {ne_phone::kId, RawDataEntry(ne_phone::kKeyMap, ne_phone::kIs102)},
      {ru_phone_aatseel::kId,
       RawDataEntry(ru_phone_aatseel::kKeyMap, ru_phone_aatseel::kIs102)},
      {ru_phone_yazhert::kId,
       RawDataEntry(ru_phone_yazhert::kKeyMap, ru_phone_yazhert::kIs102)},
      {si::kId, RawDataEntry(si::kKeyMap, si::kIs102, si::kTransforms,
                             si::kTransformsLen, si::kHistoryPrune)},
      {ta_inscript::kId,
       RawDataEntry(ta_inscript::kKeyMap, ta_inscript::kIs102)},
      {ta_itrans::kId,
       RawDataEntry(us::kKeyMap, us::kIs102, ta_itrans::kTransforms,
                    ta_itrans::kTransformsLen, ta_itrans::kHistoryPrune)},
      {ta_phone::kId,
       RawDataEntry(ta_phone::kKeyMap, ta_phone::kIs102, ta_phone::kTransforms,
                    ta_phone::kTransformsLen, ta_phone::kHistoryPrune)},
      {ta_tamil99::kId,
       RawDataEntry(ta_tamil99::kKeyMap, ta_tamil99::kIs102,
                    ta_tamil99::kTransforms, ta_tamil99::kTransformsLen,
                    ta_tamil99::kHistoryPrune)},
      {ta_typewriter::kId,
       RawDataEntry(ta_typewriter::kKeyMap, ta_typewriter::kIs102)},
      {te_phone::kId,
       RawDataEntry(us::kKeyMap, us::kIs102, te_phone::kTransforms,
                    te_phone::kTransformsLen, te_phone::kHistoryPrune)},
      {th::kId, RawDataEntry(th::kKeyMap, th::kIs102)},
      {th_pattajoti::kId,
       RawDataEntry(th_pattajoti::kKeyMap, th_pattajoti::kIs102)},
      {th_tis::kId, RawDataEntry(th_tis::kKeyMap, th_tis::kIs102)},
      {vi_tcvn::kId,
       RawDataEntry(us::kKeyMap, us::kIs102, vi_tcvn::kTransforms,
                    vi_tcvn::kTransformsLen, vi_tcvn::kHistoryPrune)},
      {vi_telex::kId,
       RawDataEntry(us::kKeyMap, us::kIs102, vi_telex::kTransforms,
                    vi_telex::kTransformsLen, vi_telex::kHistoryPrune)},
      {vi_viqr::kId,
       RawDataEntry(us::kKeyMap, us::kIs102, vi_viqr::kTransforms,
                    vi_viqr::kTransformsLen, vi_viqr::kHistoryPrune)},
      {vi_vni::kId,
       RawDataEntry(us::kKeyMap, us::kIs102, vi_vni::kTransforms,
                    vi_vni::kTransformsLen, vi_vni::kHistoryPrune)}};
  return kRawData;
}

const char* const k101Keys[] = {
    // Row #1
    "Backquote", "Digit1", "Digit2", "Digit3", "Digit4", "Digit5", "Digit6",
    "Digit7", "Digit8", "Digit9", "Digit0", "Minus", "Equal",
    // Row #2
    "KeyQ", "KeyW", "KeyE", "KeyR", "KeyT", "KeyY", "KeyU", "KeyI", "KeyO",
    "KeyP", "BracketLeft", "BracketRight", "Backslash",
    // Row #3
    "KeyA", "KeyS", "KeyD", "KeyF", "KeyG", "KeyH", "KeyJ", "KeyK", "KeyL",
    "Semicolon", "Quote",
    // Row #4
    "KeyZ", "KeyX", "KeyC", "KeyV", "KeyB", "KeyN", "KeyM", "Comma", "Period",
    "Slash",
    // Row #5
    "Space"};

const char* const k102Keys[] = {
    // Row #1
    "Backquote", "Digit1", "Digit2", "Digit3", "Digit4", "Digit5", "Digit6",
    "Digit7", "Digit8", "Digit9", "Digit0", "Minus", "Equal",
    // Row #2
    "KeyQ", "KeyW", "KeyE", "KeyR", "KeyT", "KeyY", "KeyU", "KeyI", "KeyO",
    "KeyP", "BracketLeft", "BracketRight",
    // Row #3
    "KeyA", "KeyS", "KeyD", "KeyF", "KeyG", "KeyH", "KeyJ", "KeyK", "KeyL",
    "Semicolon", "Quote", "Backslash",
    // Row #4
    "IntlBackslash", "KeyZ", "KeyX", "KeyC", "KeyV", "KeyB", "KeyN", "KeyM",
    "Comma", "Period", "Slash",
    // Row #5
    "Space"};

// Parses the raw key mappings and generate a KeyMap instance.
KeyMap ParseKeyMap(const char** raw_key_map, bool is_102) {
  const char* const* std_keys = is_102 ? k102Keys : k101Keys;
  size_t nkeys = is_102 ? base::size(k102Keys) : base::size(k101Keys);
  KeyMap key_map;
  for (size_t i = 0; i < nkeys; ++i)
    key_map[std_keys[i]] = raw_key_map[i];
  return key_map;
}

// The prefix unit can be /[...]/ or a single character
// (e.g. /[a-z]/, /[\\[\\]]/, /a/, /\\+/, etc.).
std::string WrapPrefixUnit(const std::string& re_unit) {
  return "(?:" + re_unit + "|$)";
}

// Expands the given regexp string so that it can match with prefixes.
// For example, /(abc)+[a-z]/ can be expanded as:
// /((?:a|$)(?:b|$)(?:c|$)+(?:[a-z]|$))/
// So, the string "abcabcx" can match the original regexp, and all its prefix
// can match the expanded prefix regexp. e.g. "abca".
std::string Prefixalize(const std::string& re_str) {
  // Using UTF16 string for iteration in character basis.
  base::string16 restr16 = base::UTF8ToUTF16(re_str);
  std::string ret;
  bool escape = false;
  int bracket = -1;
  int brace = -1;
  for (size_t i = 0; i < restr16.length(); ++i) {
    std::string ch = base::UTF16ToUTF8(restr16.substr(i, 1));
    if (escape) {
      if (bracket < 0) {
        ret += WrapPrefixUnit("\\" + ch);
      }
      escape = false;
      continue;
    }
    // |escape| == false.
    if (ch == "\\") {
      escape = true;
    } else if (ch == "[") {
      bracket = i;
    } else if (ch == "{" && bracket < 0) {
      brace = i;
    } else if (bracket >= 0) {
      if (ch == "]") {
        ret += WrapPrefixUnit(
            base::UTF16ToUTF8(restr16.substr(bracket, i - bracket + 1)));
        bracket = -1;
      }
    } else if (brace >= 0) {
      if (ch == "}") {
        ret += base::UTF16ToUTF8(restr16.substr(brace, i - brace + 1));
        brace = -1;
      }
    } else if (re2::RE2::FullMatch(ch, "[+*?.()|]")) {
      ret += ch;
    } else {
      ret += WrapPrefixUnit(ch);
    }
  }
  return ret;
}

// Parses the raw transform definition string and generate a transform rule map,
// a merged regexp which is used to do the quick check whether a given
// string can match one of the transform rules, and a prefix regexp which is
// used to check whether there will be future transform matches.
// |raw_transforms| is a list of strings, the strings at the even number of
// index are the regexp to define the rule, and the strings at the odd number of
// index are the string to replace the matched string. It can contains "\\1",
// "\\2", etc. to represent the strings in the matched sub groups.
// e.g. this definition can swap the 2 digits when type "~":
//      "([0-9])([0-9])~" -> "\\2\\1"
std::pair<std::unique_ptr<re2::RE2>, std::unique_ptr<re2::RE2>> ParseTransforms(
    const char** raw_transforms,
    uint16_t trans_count,
    std::map<uint16_t, TransformRule>* re_map) {
  if (!trans_count)
    return std::make_pair(nullptr, nullptr);

  DCHECK(!(trans_count & 1));

  std::string all_prefixes;
  std::string all_trans;
  uint16_t sum_of_groups = 1;
  for (uint16_t i = 0; i < trans_count; i += 2) {
    std::string from(raw_transforms[i]);
    const char* to = raw_transforms[i + 1];

    auto from_re = std::make_unique<re2::RE2>(from + "$");
    int group_count = from_re->NumberOfCapturingGroups();
    all_trans += "(" + from + "$)|";
    all_prefixes += Prefixalize(from) + "|";

    (*re_map)[sum_of_groups] = std::make_pair(std::move(from_re), to);
    sum_of_groups += group_count + 1;
  }
  return std::make_pair(
      std::make_unique<re2::RE2>(all_trans.substr(0, all_trans.length() - 1)),
      std::make_unique<re2::RE2>(
          all_prefixes.substr(0, all_prefixes.length() - 1)));
}

// Parses the history prune regexp and returns the RE2 instance.
// This regexp is used by the caller code instead of in RulesData.
std::unique_ptr<re2::RE2> ParseHistoryPrune(const char* history_prune) {
  if (!history_prune)
    return nullptr;
  return std::make_unique<re2::RE2>(std::string("^(") + history_prune + ")$");
}

// The delimit inserted at the position of "transat".
// The term "transat" means "was transformed at".
// Please refer to some details in the |Transform| method.
static const char* kTransatDelimit = u8"\u001D";

}  // namespace

RulesData::RulesData() = default;
RulesData::~RulesData() = default;

// static
std::unique_ptr<RulesData> RulesData::Create(const char*** key_map,
                                             bool is_102_keyboard,
                                             const char** transforms,
                                             const uint16_t transforms_count,
                                             const char* history_prune) {
  std::unique_ptr<RulesData> data = std::make_unique<RulesData>();
  for (uint8_t i = 0; i < kKeyMapCount; ++i) {
    data->key_maps_[i] = ParseKeyMap(key_map[i], is_102_keyboard);
  }
  auto regexes =
      ParseTransforms(transforms, transforms_count, &data->transform_rules_);
  data->transform_re_merged_ = std::move(regexes.first);
  data->prefix_re_ = std::move(regexes.second);
  data->history_prune_re_ = ParseHistoryPrune(history_prune);
  return data;
}

// static
std::unique_ptr<RulesData> RulesData::GetById(const std::string& id) {
  const std::map<std::string, RawDataEntry>& raw_data = GetRawData();
  auto it = raw_data.find(id);
  if (it == raw_data.end())
    return nullptr;

  const RawDataEntry& entry = it->second;
  return Create(entry.key_map, entry.is_102_keyboard, entry.transforms,
                entry.transforms_count, entry.history_prune);
}

// static
bool RulesData::IsIdSupported(const std::string& id) {
  return base::Contains(GetRawData(), id);
}

const KeyMap* RulesData::GetKeyMapByModifiers(uint8_t modifiers) const {
  return modifiers < kKeyMapCount ? &key_maps_[modifiers] : nullptr;
}

bool RulesData::Transform(const std::string& context,
                          int transat,
                          const std::string& appended,
                          std::string* transformed) const {
  // Inserts the transat delimit if |transat| indicates a valid position.
  // The rule definition may contains this delimit to explicit match a
  // transformed character. e.g. "a" -> "X", "X\u001Da" -> "aa". So when typing
  // "a", it will be transformed as "X". And typing "a" again can be transformed
  // as "aa". In that case, the caller must pass in the |transat| value as 1 in
  // order to match the regexp "X\u001Da". And if there was a "X" in the input
  // field, the caller passes |transat| as -1, therefore typing "a" won't
  // trigger the transform.
  std::string str = transat > 0 ? context.substr(0, transat) + kTransatDelimit +
                                      context.substr(transat) + appended
                                : context + appended;
  int nmatch = transform_re_merged_->NumberOfCapturingGroups();
  auto matches = std::make_unique<re2::StringPiece[]>(nmatch);
  bool is_matched = transform_re_merged_->Match(str, 0, str.length(),
                                                re2::RE2::Anchor::UNANCHORED,
                                                matches.get(), nmatch);
  if (!is_matched)
    return false;

  int pos = 1;
  while (pos < nmatch && !matches[pos].data())
    ++pos;

  const auto& found = transform_rules_.find(pos);
  if (found == transform_rules_.end())
    return false;

  auto& rule = found->second;
  auto& re = *(std::get<0>(rule));
  const std::string& repl = std::get<1>(rule);

  // Don't transform if matching happens in the middle of |appended| string.
  if (re.Match(str, 0, str.length(), re2::RE2::Anchor::UNANCHORED,
               matches.get(), nmatch)) {
    size_t match_start = matches[0].data() - str.data();
    if (match_start > str.length() - appended.length())
      return false;
  }

  re2::RE2::Replace(&str, re, repl);
  re2::RE2::Replace(&str, "\u001d", "");
  *transformed = str;

  return true;
}

bool RulesData::MatchHistoryPrune(const std::string& str) const {
  return history_prune_re_ && re2::RE2::FullMatch(str, *history_prune_re_);
}

bool RulesData::PredictTransform(const std::string& str, int transat) const {
  if (!prefix_re_)
    return false;

  std::string s = transat > 0 ? str.substr(0, transat) + kTransatDelimit +
                                    str.substr(transat)
                              : str;
  // Try to match the prefix re by all of the suffix of the context string.
  size_t len = str.length();
  for (size_t i = 0; i < len; ++i) {
    std::string suffix = s.substr(len - i - 1);
    if (re2::RE2::FullMatch(suffix, *prefix_re_))
      return true;
  }
  return false;
}

}  // namespace rulebased
}  // namespace ime
}  // namespace chromeos
