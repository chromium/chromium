// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/json_matchers.h"

#include <stddef.h>

#include <algorithm>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/json/json_writer.h"
#include "base/memory/stack_allocated.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace updater::test::json {

namespace {

// A matcher in this file starts its explanation with the path of the value it
// explains, from the value that the outermost matcher in this file matches,
// and with that value, as in `.request.apps[1].ping.ad: is 5098, expected
// absent`. The outermost matcher explains its own value without a path, as in
// `expected 3`, since GMock prints that value.
//
// A matcher inside another matcher in this file marks its explanation with
// `kBegin` and `kEnd`. `testing::AllOf()` and `testing::AnyOf()` can join it
// with the explanations of other GMock matchers, such as `testing::Eq()`, which
// have no path. `ExplainAt()` in the enclosing matcher removes the marks and
// adds the path to the explanations without marks.

// Mark the start and the end of the explanation of a nested matcher.
// `base::WriteJson()` escapes them in values.
constexpr std::string_view kBegin = "\x02";
constexpr std::string_view kEnd = "\x03";

// Joins explanations in `testing::AllOf()` and `testing::AnyOf()`.
constexpr std::string_view kAnd = ", and ";

// The value that the outermost matcher in this file matches while it explains
// a match, or null.
constinit thread_local const base::Value* g_root = nullptr;

// Returns true and appends the path of `target` in `value` to `path` if `value`
// is or contains `target`.
bool FindPath(const base::Value& value,
              const base::Value& target,
              std::string& path) {
  if (&value == &target) {
    return true;
  }
  const size_t size = path.size();
  if (const base::DictValue* dict = value.GetIfDict()) {
    for (const auto [key, child] : *dict) {
      base::StrAppend(&path, {".", key});
      if (FindPath(child, target, path)) {
        return true;
      }
      path.resize(size);
    }
  } else if (const base::ListValue* list = value.GetIfList()) {
    for (size_t i = 0; i < list->size(); ++i) {
      base::StrAppend(&path, {"[", base::NumberToString(i), "]"});
      if (FindPath((*list)[i], target, path)) {
        return true;
      }
      path.resize(size);
    }
  }
  return false;
}

// Finds the path of the value that a matcher in this file matches, while the
// matcher explains the match. Makes the value the root while in scope, unless
// the root contains it.
class ScopedMatch {
  STACK_ALLOCATED();

 public:
  explicit ScopedMatch(const base::Value& value)
      : nested_(g_root != nullptr), previous_root_(g_root) {
    // A value that the root doesn't contain, such as a copy, gets paths from
    // itself.
    if (!g_root || !FindPath(*g_root, value, path_)) {
      g_root = &value;
    }
  }
  ScopedMatch(const ScopedMatch&) = delete;
  ScopedMatch& operator=(const ScopedMatch&) = delete;
  ~ScopedMatch() { g_root = previous_root_; }

  // Returns the path of the value, as in `.request.apps[1]`, or "" for the
  // root.
  const std::string& path() const { return path_; }

  // Returns `explanation` for the listener of the matcher, marked if the
  // matcher is inside another matcher in this file.
  std::string Finish(std::string explanation) const {
    if (!nested_ || explanation.empty()) {
      return explanation;
    }
    return base::StrCat({kBegin, explanation, kEnd});
  }

 private:
  const bool nested_;
  const base::Value* const previous_root_;
  std::string path_;
};

// Returns `value` as compact JSON.
std::string ToJson(const base::Value& value) {
  return base::WriteJson(value).value_or("<binary>");
}

// Returns `value` as compact JSON, shortened for an explanation.
std::string ToShortJson(const base::Value& value) {
  static constexpr size_t kMaxLength = 200;
  const std::string json = ToJson(value);
  if (json.size() <= kMaxLength) {
    return json;
  }
  return base::StrCat({base::TruncateUTF8ToByteSize(json, kMaxLength), "..."});
}

// A part of an explanation, from a matcher in this file if `marked`.
struct ExplanationPart {
  std::string_view text;
  bool marked = false;
};

// Returns the parts of `explanation`, without their marks.
std::vector<ExplanationPart> SplitExplanation(std::string_view explanation) {
  std::vector<ExplanationPart> parts;
  while (!explanation.empty()) {
    const size_t begin = std::min(explanation.find(kBegin), explanation.size());
    if (begin > 0) {
      parts.push_back({.text = explanation.substr(0, begin)});
      explanation.remove_prefix(begin);
      continue;
    }
    explanation.remove_prefix(kBegin.size());
    const size_t end = std::min(explanation.find(kEnd), explanation.size());
    parts.push_back({.text = explanation.substr(0, end), .marked = true});
    explanation.remove_prefix(std::min(end + kEnd.size(), explanation.size()));
  }
  return parts;
}

// Returns the explanation for `value`, found at `path`, given the
// `explanation` from its matchers. Removes the marks from the explanations of
// matchers in this file, and starts the explanations of other GMock matchers
// with `path` and `value`, unless `path` is empty.
std::string ExplainAt(std::string_view path,
                      const base::Value& value,
                      std::string_view explanation) {
  std::string prefix;
  // Returns `text`, from another GMock matcher, after `path` and `value`.
  const auto explain_value = [&](std::string_view text) {
    if (path.empty()) {
      return std::string(text);
    }
    if (prefix.empty()) {
      prefix = base::StrCat({path, ": is ", ToShortJson(value)});
    }
    return text.empty() ? prefix : base::StrCat({prefix, ", ", text});
  };
  if (explanation.empty()) {
    return explain_value("");
  }
  const std::vector<ExplanationPart> parts = SplitExplanation(explanation);
  std::string result;
  for (size_t i = 0; i < parts.size(); ++i) {
    std::string_view text = parts[i].text;
    if (parts[i].marked) {
      result.append(text);
    } else if (i == 0) {
      result.append(explain_value(text));
    } else if (text.starts_with(kAnd)) {
      // `testing::AllOf()` or `testing::AnyOf()` joins this explanation of
      // `value` to the marked one before it.
      text.remove_prefix(kAnd.size());
      result.append(kAnd);
      if (!text.empty()) {
        result.append(explain_value(text));
      }
    } else {
      // Keeps the words that other GMock matchers, such as
      // `testing::ElementsAre()`, write after a marked explanation.
      result.append(text);
    }
  }
  return result;
}

// Matches a dict that has `key`, with a value that matches `matcher` if set.
class HasKeyMatcher {
 public:
  using is_gtest_matcher = void;

  HasKeyMatcher(std::string key, std::optional<ValueMatcher> matcher)
      : key_(std::move(key)), matcher_(std::move(matcher)) {}

  bool MatchAndExplain(const base::Value& value,
                       testing::MatchResultListener* listener) const {
    const base::DictValue* dict = value.GetIfDict();
    const base::Value* child = dict ? dict->Find(key_) : nullptr;
    if (!listener->IsInterested()) {
      return child && (!matcher_ || matcher_->Matches(*child));
    }
    const ScopedMatch match(value);
    if (!dict) {
      *listener << match.Finish(
          ExplainAt(match.path(), value, "which is not a dict"));
      return false;
    }
    const std::string path = base::StrCat({match.path(), ".", key_});
    if (!child) {
      *listener << match.Finish(base::StrCat({path, ": is missing"}));
      return false;
    }
    testing::StringMatchResultListener child_listener;
    const bool matches =
        !matcher_ || matcher_->MatchAndExplain(*child, &child_listener);
    *listener << match.Finish(ExplainAt(path, *child, child_listener.str()));
    return matches;
  }

  void DescribeTo(std::ostream* os) const {
    *os << "has key \"" << key_ << "\"";
    if (matcher_) {
      *os << " that ";
      matcher_->DescribeTo(os);
    }
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "doesn't have key \"" << key_ << "\"";
    if (matcher_) {
      *os << ", or has it with a value that ";
      matcher_->DescribeNegationTo(os);
    }
  }

 private:
  std::string key_;
  std::optional<ValueMatcher> matcher_;
};

// Matches a dict that doesn't have `key`.
class LacksKeyMatcher {
 public:
  using is_gtest_matcher = void;

  explicit LacksKeyMatcher(std::string key) : key_(std::move(key)) {}

  bool MatchAndExplain(const base::Value& value,
                       testing::MatchResultListener* listener) const {
    const base::DictValue* dict = value.GetIfDict();
    const base::Value* child = dict ? dict->Find(key_) : nullptr;
    if (!listener->IsInterested()) {
      return dict && !child;
    }
    const ScopedMatch match(value);
    if (!dict) {
      *listener << match.Finish(
          ExplainAt(match.path(), value, "which is not a dict"));
      return false;
    }
    const std::string path = base::StrCat({match.path(), ".", key_});
    *listener << match.Finish(child ? ExplainAt(path, *child, "expected absent")
                                    : base::StrCat({path, ": is missing"}));
    return !child;
  }

  void DescribeTo(std::ostream* os) const {
    *os << "doesn't have key \"" << key_ << "\"";
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "isn't a dict, or has key \"" << key_ << "\"";
  }

 private:
  std::string key_;
};

// Matches a string that matches `matcher`.
class IsStringMatcher {
 public:
  using is_gtest_matcher = void;

  explicit IsStringMatcher(testing::Matcher<const std::string&> matcher)
      : matcher_(std::move(matcher)) {}

  bool MatchAndExplain(const base::Value& value,
                       testing::MatchResultListener* listener) const {
    if (!listener->IsInterested()) {
      return value.is_string() && matcher_.Matches(value.GetString());
    }
    const ScopedMatch match(value);
    if (!value.is_string()) {
      *listener << match.Finish(
          ExplainAt(match.path(), value, "which is not a string"));
      return false;
    }
    testing::StringMatchResultListener string_listener;
    const bool matches =
        matcher_.MatchAndExplain(value.GetString(), &string_listener);
    *listener << match.Finish(
        ExplainAt(match.path(), value, string_listener.str()));
    return matches;
  }

  void DescribeTo(std::ostream* os) const {
    *os << "is a string that ";
    matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "isn't a string, or is a string that ";
    matcher_.DescribeNegationTo(os);
  }

 private:
  testing::Matcher<const std::string&> matcher_;
};

// Matches a list that matches `matcher`.
class IsListMatcher {
 public:
  using is_gtest_matcher = void;

  explicit IsListMatcher(testing::Matcher<const base::ListValue&> matcher)
      : matcher_(std::move(matcher)) {}

  bool MatchAndExplain(const base::Value& value,
                       testing::MatchResultListener* listener) const {
    if (!listener->IsInterested()) {
      return value.is_list() && matcher_.Matches(value.GetList());
    }
    const ScopedMatch match(value);
    if (!value.is_list()) {
      *listener << match.Finish(
          ExplainAt(match.path(), value, "which is not a list"));
      return false;
    }
    testing::StringMatchResultListener list_listener;
    const bool matches =
        matcher_.MatchAndExplain(value.GetList(), &list_listener);
    *listener << match.Finish(
        ExplainAt(match.path(), value, list_listener.str()));
    return matches;
  }

  void DescribeTo(std::ostream* os) const {
    *os << "is a list that ";
    matcher_.DescribeTo(os);
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "isn't a list, or is a list that ";
    matcher_.DescribeNegationTo(os);
  }

 private:
  testing::Matcher<const base::ListValue&> matcher_;
};

// Matches a value equal to `expected`.
class EqualsMatcher {
 public:
  using is_gtest_matcher = void;

  explicit EqualsMatcher(base::Value expected)
      : expected_(std::move(expected)) {}

  bool MatchAndExplain(const base::Value& value,
                       testing::MatchResultListener* /* listener */) const {
    return value == expected_;
  }

  void DescribeTo(std::ostream* os) const {
    *os << "is equal to " << ToJson(expected_);
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "isn't equal to " << ToJson(expected_);
  }

 private:
  base::Value expected_;
};

// The string value at `key` that identifies a list entry, such as the `appid`
// of an app. `key` must outlive the matchers that use it.
struct EntryId {
  std::string_view key;
  std::string value;
};

// Matches a dict with a list at the dotted `list_path` that has an entry that
// matches `matcher`, if set. If `id` is set, only dict entries whose string at
// `id->key` equals `id->value` case-insensitively count. `noun` names an entry
// in messages, as in "app", and must outlive the matcher.
class EntryMatcher {
 public:
  using is_gtest_matcher = void;

  EntryMatcher(std::string_view list_path,
               std::string_view noun,
               std::optional<EntryId> id,
               std::optional<ValueMatcher> matcher)
      : keys_(base::SplitString(list_path,
                                ".",
                                base::KEEP_WHITESPACE,
                                base::SPLIT_WANT_ALL)),
        noun_(noun),
        id_(std::move(id)),
        matcher_(std::move(matcher)) {}

  bool MatchAndExplain(const base::Value& value,
                       testing::MatchResultListener* listener) const {
    if (!listener->IsInterested()) {
      const base::ListValue* list = FindList(value);
      return list &&
             std::ranges::any_of(*list, [this](const base::Value& entry) {
               return IsCandidate(entry) &&
                      (!matcher_ || matcher_->Matches(entry));
             });
    }
    const ScopedMatch match(value);
    std::string path = match.path();
    const base::Value* list_value = &value;
    for (const std::string& key : keys_) {
      const base::DictValue* dict = list_value->GetIfDict();
      if (!dict) {
        *listener << match.Finish(
            ExplainAt(path, *list_value, "which is not a dict"));
        return false;
      }
      base::StrAppend(&path, {".", key});
      list_value = dict->Find(key);
      if (!list_value) {
        *listener << match.Finish(base::StrCat({path, ": is missing"}));
        return false;
      }
    }
    const base::ListValue* list = list_value->GetIfList();
    if (!list) {
      *listener << match.Finish(
          ExplainAt(path, *list_value, "which is not a list"));
      return false;
    }

    std::vector<std::string> other_ids;
    std::vector<std::string> mismatches;
    for (size_t i = 0; i < list->size(); ++i) {
      const base::Value& entry = (*list)[i];
      if (!IsCandidate(entry)) {
        if (const std::string* entry_id = GetId(entry)) {
          other_ids.push_back(base::StrCat({"\"", *entry_id, "\""}));
        }
        continue;
      }
      testing::StringMatchResultListener entry_listener;
      const bool matches =
          !matcher_ || matcher_->MatchAndExplain(entry, &entry_listener);
      std::string explanation =
          ExplainAt(base::StrCat({path, "[", base::NumberToString(i), "]"}),
                    entry, entry_listener.str());
      if (matches) {
        *listener << match.Finish(std::move(explanation));
        return true;
      }
      mismatches.push_back(std::move(explanation));
    }

    std::string explanation;
    if (mismatches.size() == 1) {
      explanation = std::move(mismatches.front());
    } else if (!mismatches.empty()) {
      explanation =
          base::StrCat({path, ": no ", DescribeNoun(),
                        " matches: ", base::JoinString(mismatches, "; ")});
    } else if (id_) {
      explanation = base::StrCat({path, ": has no ", DescribeNoun()});
      if (!other_ids.empty()) {
        base::StrAppend(&explanation,
                        {", only ", base::JoinString(other_ids, ", ")});
      }
    } else {
      explanation = base::StrCat({path, ": is empty"});
    }
    *listener << match.Finish(std::move(explanation));
    return false;
  }

  void DescribeTo(std::ostream* os) const {
    *os << (id_ ? "has " : "has at least one ");
    DescribeEntryTo(os);
  }

  void DescribeNegationTo(std::ostream* os) const {
    *os << "has no ";
    DescribeEntryTo(os);
  }

 private:
  // Returns the list at `keys_` in `value`, or null if there is none.
  const base::ListValue* FindList(const base::Value& value) const {
    const base::Value* list_value = &value;
    for (const std::string& key : keys_) {
      const base::DictValue* dict = list_value->GetIfDict();
      list_value = dict ? dict->Find(key) : nullptr;
      if (!list_value) {
        return nullptr;
      }
    }
    return list_value->GetIfList();
  }

  // Returns the id of `entry`, or null if it has none or `id_` isn't set.
  const std::string* GetId(const base::Value& entry) const {
    const base::DictValue* dict = entry.GetIfDict();
    return id_ && dict ? dict->FindString(id_->key) : nullptr;
  }

  // Returns true if `matcher_` should match `entry`: if `id_` is set, only if
  // `entry` has that id.
  bool IsCandidate(const base::Value& entry) const {
    if (!id_) {
      return true;
    }
    const std::string* entry_id = GetId(entry);
    return entry_id && base::EqualsCaseInsensitiveASCII(*entry_id, id_->value);
  }

  // Returns the noun for the entry, with its id if set, as in `app "test1"`.
  std::string DescribeNoun() const {
    return id_ ? base::StrCat({noun_, " \"", id_->value, "\""})
               : std::string(noun_);
  }

  // Describes the entry, as in `app "test1" that ...` or `event that ...`.
  void DescribeEntryTo(std::ostream* os) const {
    *os << DescribeNoun();
    if (matcher_) {
      *os << " that ";
      matcher_->DescribeTo(os);
    }
  }

  std::vector<std::string> keys_;
  std::string_view noun_;
  std::optional<EntryId> id_;
  std::optional<ValueMatcher> matcher_;
};

// Returns a matcher for a dict with a value at the dotted `path` that matches
// `matcher`, if set.
ValueMatcher HasPathMatcher(std::string_view path,
                            std::optional<ValueMatcher> matcher) {
  std::vector<std::string> keys =
      base::SplitString(path, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  // `base::SplitString()` returns no keys for an empty `path`.
  CHECK(!keys.empty() &&
        std::ranges::none_of(
            keys, [](const std::string& key) { return key.empty(); }))
      << "Invalid path: \"" << path << "\"";
  std::optional<ValueMatcher> result = std::move(matcher);
  for (std::string& key : base::Reversed(keys)) {
    result = HasKeyMatcher(std::move(key), std::move(result));
  }
  return *std::move(result);
}

}  // namespace

namespace internal {

ValueMatcher HasKeyThat(std::string key, ValueMatcher matcher) {
  return HasKeyMatcher(std::move(key), std::move(matcher));
}

ValueMatcher HasPathThat(std::string_view path, ValueMatcher matcher) {
  return HasPathMatcher(path, std::move(matcher));
}

ValueMatcher Equals(base::Value expected) {
  return EqualsMatcher(std::move(expected));
}

}  // namespace internal

ValueMatcher HasKey(std::string key) {
  return HasKeyMatcher(std::move(key), std::nullopt);
}

ValueMatcher LacksKey(std::string key) {
  return LacksKeyMatcher(std::move(key));
}

ValueMatcher HasPath(std::string_view path) {
  return HasPathMatcher(path, std::nullopt);
}

ValueMatcher IsString(testing::Matcher<const std::string&> matcher) {
  return IsStringMatcher(std::move(matcher));
}

ValueMatcher IsList(testing::Matcher<const base::ListValue&> matcher) {
  return IsListMatcher(std::move(matcher));
}

ValueMatcher UpdateRequest(ValueMatcher matcher) {
  return HasKeyMatcher("request", std::move(matcher));
}

ValueMatcher HasApp(std::string app_id) {
  return EntryMatcher("request.apps", "app",
                      EntryId{.key = "appid", .value = std::move(app_id)},
                      std::nullopt);
}

ValueMatcher HasApp(std::string app_id, ValueMatcher matcher) {
  return EntryMatcher("request.apps", "app",
                      EntryId{.key = "appid", .value = std::move(app_id)},
                      std::move(matcher));
}

ValueMatcher HasEvent(ValueMatcher matcher) {
  return EntryMatcher("events", "event", std::nullopt, std::move(matcher));
}

}  // namespace updater::test::json
