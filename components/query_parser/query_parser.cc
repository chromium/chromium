// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_parser/query_parser.h"

#include <algorithm>
#include <memory>
#include <ostream>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/i18n/break_iterator.h"
#include "base/i18n/case_conversion.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"

namespace query_parser {
namespace {

// Returns true if |mp1.first| is less than |mp2.first|. This is used to
// sort match positions.
int CompareMatchPosition(const Snippet::MatchPosition& mp1,
                         const Snippet::MatchPosition& mp2) {
  return mp1.first < mp2.first;
}

// Returns true if |mp2| intersects |mp1|. This is intended for use by
// CoalesceMatchesFrom and isn't meant as a general intersection comparison
// function.
bool SnippetIntersects(const Snippet::MatchPosition& mp1,
                       const Snippet::MatchPosition& mp2) {
  return mp2.first >= mp1.first && mp2.first <= mp1.second;
}

// Coalesces match positions in |matches| after index that intersect the match
// position at |index|.
void CoalesceMatchesFrom(size_t index, Snippet::MatchPositions* matches) {
  Snippet::MatchPosition& mp = (*matches)[index];
  for (auto i = matches->begin() + index + 1; i != matches->end();) {
    if (SnippetIntersects(mp, *i)) {
      mp.second = std::max(mp.second, i->second);
      i = matches->erase(i);
    } else {
      return;
    }
  }
}

// Returns true if the character is considered a quote.
bool IsQueryQuote(wchar_t ch) {
  return ch == '"' ||
         ch == 0xab ||    // left pointing double angle bracket
         ch == 0xbb ||    // right pointing double angle bracket
         ch == 0x201c ||  // left double quotation mark
         ch == 0x201d ||  // right double quotation mark
         ch == 0x201e;    // double low-9 quotation mark
}

}  // namespace

// Inheritance structure:
// Queries are represented as trees of QueryNodes.
// QueryNodes are either a collection of subnodes (a QueryNodeList)
// or a single word (a QueryNodeWord).

// A QueryNodeWord is a single word in the query.
class QueryNodeWord : public QueryNode {
 public:
  explicit QueryNodeWord(const std::u16string& word,
                         MatchingAlgorithm matching_algorithm);

  QueryNodeWord(const QueryNodeWord&) = delete;
  QueryNodeWord& operator=(const QueryNodeWord&) = delete;

  ~QueryNodeWord() override;

  const std::u16string& word() const { return word_; }

  bool literal() const { return literal_; }
  void set_literal(bool literal) { literal_ = literal; }

  // QueryNode:
  int AppendToSQLiteQuery(std::u16string* query) const override;
  bool IsWord() const override;
  bool Matches(const std::u16string& word, bool exact) const override;
  bool HasMatchIn(const QueryWordVector& words,
                  Snippet::MatchPositions* match_positions) const override;
  bool HasMatchIn(const QueryWordVector& words, bool exact) const override;
  void AppendWords(std::vector<std::u16string>* words) const override;

 private:
  std::u16string word_;
  bool literal_;
  const MatchingAlgorithm matching_algorithm_;
};

QueryNodeWord::QueryNodeWord(const std::u16string& word,
                             MatchingAlgorithm matching_algorithm)
    : word_(word), literal_(false), matching_algorithm_(matching_algorithm) {}

QueryNodeWord::~QueryNodeWord() = default;

int QueryNodeWord::AppendToSQLiteQuery(std::u16string* query) const {
  query->append(word_);

  // Use prefix search if we're not literal and long enough.
  if (!literal_ &&
      QueryParser::IsWordLongEnoughForPrefixSearch(word_, matching_algorithm_))
    *query += L'*';
  return 1;
}

bool QueryNodeWord::IsWord() const {
  return true;
}

bool QueryNodeWord::Matches(const std::u16string& word, bool exact) const {
  if (exact ||
      !QueryParser::IsWordLongEnoughForPrefixSearch(word_, matching_algorithm_))
    return word == word_;
  return word.size() >= word_.size() &&
         (word_.compare(0, word_.size(), word, 0, word_.size()) == 0);
}

bool QueryNodeWord::HasMatchIn(const QueryWordVector& words,
                               Snippet::MatchPositions* match_positions) const {
  bool matched = false;
  for (const auto& queryWord : words) {
    if (Matches(queryWord.word, false)) {
      size_t match_start = queryWord.position;
      match_positions->push_back(Snippet::MatchPosition(
          match_start, match_start + static_cast<int>(word_.size())));
      matched = true;
    }
  }
  return matched;
}

bool QueryNodeWord::HasMatchIn(const QueryWordVector& words, bool exact) const {
  return base::ranges::any_of(words, [&](const auto& query_word) {
    return Matches(query_word.word, exact);
  });
}

void QueryNodeWord::AppendWords(std::vector<std::u16string>* words) const {
  words->push_back(word_);
}

// A QueryNodeList has a collection of QueryNodes which are deleted in the end.
class QueryNodeList : public QueryNode {
 public:
  QueryNodeList();

  QueryNodeList(const QueryNodeList&) = delete;
  QueryNodeList& operator=(const QueryNodeList&) = delete;

  ~QueryNodeList() override;

  QueryNodeVector* children() { return &children_; }

  void AddChild(std::unique_ptr<QueryNode> node);

  // Remove empty subnodes left over from other parsing.
  void RemoveEmptySubnodes();

  // QueryNode:
  int AppendToSQLiteQuery(std::u16string* query) const override;
  bool IsWord() const override;
  bool Matches(const std::u16string& word, bool exact) const override;
  bool HasMatchIn(const QueryWordVector& words,
                  Snippet::MatchPositions* match_positions) const override;
  bool HasMatchIn(const QueryWordVector& words, bool exact) const override;
  void AppendWords(std::vector<std::u16string>* words) const override;

 protected:
  int AppendChildrenToString(std::u16string* query) const;

  QueryNodeVector children_;
};

QueryNodeList::QueryNodeList() = default;

QueryNodeList::~QueryNodeList() {
}

void QueryNodeList::AddChild(std::unique_ptr<QueryNode> node) {
  children_.push_back(std::move(node));
}

void QueryNodeList::RemoveEmptySubnodes() {
  QueryNodeVector kept_children;
  for (size_t i = 0; i < children_.size(); ++i) {
    if (children_[i]->IsWord()) {
      kept_children.push_back(std::move(children_[i]));
      continue;
    }

    QueryNodeList* list_node = static_cast<QueryNodeList*>(children_[i].get());
    list_node->RemoveEmptySubnodes();
    if (!list_node->children()->empty())
      kept_children.push_back(std::move(children_[i]));
  }
  children_.swap(kept_children);
}

int QueryNodeList::AppendToSQLiteQuery(std::u16string* query) const {
  return AppendChildrenToString(query);
}

bool QueryNodeList::IsWord() const {
  return false;
}

bool QueryNodeList::Matches(const std::u16string& word, bool exact) const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool QueryNodeList::HasMatchIn(const QueryWordVector& words,
                               Snippet::MatchPositions* match_positions) const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool QueryNodeList::HasMatchIn(const QueryWordVector& words, bool exact) const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

void QueryNodeList::AppendWords(std::vector<std::u16string>* words) const {
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->AppendWords(words);
}

int QueryNodeList::AppendChildrenToString(std::u16string* query) const {
  int num_words = 0;
  for (auto node = children_.begin(); node != children_.end(); ++node) {
    if (node != children_.begin())
      query->push_back(L' ');
    num_words += (*node)->AppendToSQLiteQuery(query);
  }
  return num_words;
}

// A QueryNodePhrase is a phrase query ("quoted").
class QueryNodePhrase : public QueryNodeList {
 public:
  QueryNodePhrase();

  QueryNodePhrase(const QueryNodePhrase&) = delete;
  QueryNodePhrase& operator=(const QueryNodePhrase&) = delete;

  ~QueryNodePhrase() override;

  // QueryNodeList:
  int AppendToSQLiteQuery(std::u16string* query) const override;
  bool HasMatchIn(const QueryWordVector& words,
                  Snippet::MatchPositions* match_positions) const override;
  bool HasMatchIn(const QueryWordVector& words, bool exact) const override;

 private:
  bool MatchesAll(const QueryWordVector& words,
                  const QueryWord** first_word,
                  const QueryWord** last_word) const;
};

QueryNodePhrase::QueryNodePhrase() = default;

QueryNodePhrase::~QueryNodePhrase() = default;

int QueryNodePhrase::AppendToSQLiteQuery(std::u16string* query) const {
  query->push_back(L'"');
  int num_words = AppendChildrenToString(query);
  query->push_back(L'"');
  return num_words;
}

bool QueryNodePhrase::MatchesAll(const QueryWordVector& words,
                                 const QueryWord** first_word,
                                 const QueryWord** last_word) const {
  if (words.size() < children_.size())
    return false;

  for (size_t i = 0, max = words.size() - children_.size() + 1; i < max; ++i) {
    bool matched_all = true;
    for (size_t j = 0; j < children_.size(); ++j) {
      if (!children_[j]->Matches(words[i + j].word, true)) {
        matched_all = false;
        break;
      }
    }
    if (matched_all) {
      *first_word = &words[i];
      *last_word = &words[i + children_.size() - 1];
      return true;
    }
  }
  return false;
}

bool QueryNodePhrase::HasMatchIn(
    const QueryWordVector& words,
    Snippet::MatchPositions* match_positions) const {
  const QueryWord* first_word;
  const QueryWord* last_word;

  if (MatchesAll(words, &first_word, &last_word)) {
    match_positions->push_back(
        Snippet::MatchPosition(first_word->position,
                               last_word->position + last_word->word.length()));
      return true;
  }
  return false;
}

bool QueryNodePhrase::HasMatchIn(const QueryWordVector& words,
                                 bool exact) const {
  const QueryWord* first_word;
  const QueryWord* last_word;
  return MatchesAll(words, &first_word, &last_word);
}

// static
bool QueryParser::IsWordLongEnoughForPrefixSearch(
    const std::u16string& word,
    MatchingAlgorithm matching_algorithm) {
  if (matching_algorithm == MatchingAlgorithm::ALWAYS_PREFIX_SEARCH)
    return true;

  DCHECK(!word.empty());
  size_t minimum_length = 3;
  // We intentionally exclude Hangul Jamos (both Conjoining and compatibility)
  // because they 'behave like' Latin letters. Moreover, we should
  // normalize the former before reaching here.
  if (0xAC00 <= word[0] && word[0] <= 0xD7A3)
    minimum_length = 2;
  return word.size() >= minimum_length;
}

// static
int QueryParser::ParseQuery(const std::u16string& query,
                            MatchingAlgorithm matching_algorithm,
                            std::u16string* sqlite_query) {
  QueryNodeList root;
  if (!ParseQueryImpl(query, matching_algorithm, &root))
    return 0;
  return root.AppendToSQLiteQuery(sqlite_query);
}

// static
void QueryParser::ParseQueryWords(const std::u16string& query,
                                  MatchingAlgorithm matching_algorithm,
                                  std::vector<std::u16string>* words) {
  QueryNodeList root;
  if (!ParseQueryImpl(query, matching_algorithm, &root))
    return;
  root.AppendWords(words);
}

// static
void QueryParser::ParseQueryNodes(const std::u16string& query,
                                  MatchingAlgorithm matching_algorithm,
                                  QueryNodeVector* nodes) {
  QueryNodeList root;
  if (ParseQueryImpl(base::i18n::ToLower(query), matching_algorithm, &root))
    nodes->swap(*root.children());
}

// static
bool QueryParser::DoesQueryMatch(const std::u16string& find_in_text,
                                 const QueryNodeVector& find_nodes,
                                 Snippet::MatchPositions* match_positions) {
  if (find_nodes.empty())
    return false;

  QueryWordVector query_words;
  std::u16string lower_find_in_text = base::i18n::ToLower(find_in_text);
  ExtractQueryWords(lower_find_in_text, &query_words);

  if (query_words.empty())
    return false;

  Snippet::MatchPositions matches;
  for (auto& find_node : find_nodes) {
    if (!find_node->HasMatchIn(query_words, &matches))
      return false;
  }
  if (lower_find_in_text.length() != find_in_text.length()) {
    // The lower case string differs from the original string. The matches are
    // meaningless.
    // TODO(sky): we need a better way to align the positions so that we don't
    // completely punt here.
    match_positions->clear();
  } else {
    SortAndCoalesceMatchPositions(&matches);
    match_positions->swap(matches);
  }
  return true;
}

// static
bool QueryParser::DoesQueryMatch(const QueryWordVector& find_in_words,
                                 const QueryNodeVector& find_nodes,
                                 bool exact) {
  if (find_nodes.empty() || find_in_words.empty())
    return false;
  return base::ranges::all_of(find_nodes, [&](const auto& find_node) {
    return find_node->HasMatchIn(find_in_words, exact);
  });
}

// static
bool QueryParser::ParseQueryImpl(const std::u16string& query,
                                 MatchingAlgorithm matching_algorithm,
                                 QueryNodeList* root) {
  base::i18n::BreakIterator iter(query, base::i18n::BreakIterator::BREAK_WORD);
  // TODO(evanm): support a locale here
  if (!iter.Init())
    return false;

  // To handle nesting, we maintain a stack of QueryNodeLists.
  // The last element (back) of the stack contains the current, deepest node.
  std::vector<QueryNodeList*> query_stack;
  query_stack.push_back(root);

  bool in_quotes = false;  // whether we're currently in a quoted phrase
  while (iter.Advance()) {
    // Just found a span between 'prev' (inclusive) and 'pos' (exclusive). It
    // is not necessarily a word, but could also be a sequence of punctuation
    // or whitespace.
    if (iter.IsWord()) {
      std::unique_ptr<QueryNodeWord> word_node =
          std::make_unique<QueryNodeWord>(iter.GetString(), matching_algorithm);
      if (in_quotes)
        word_node->set_literal(true);
      query_stack.back()->AddChild(std::move(word_node));
    } else {  // Punctuation.
      if (IsQueryQuote(query[iter.prev()])) {
        if (!in_quotes) {
          std::unique_ptr<QueryNodeList> quotes_node =
              std::make_unique<QueryNodePhrase>();
          QueryNodeList* quotes_node_ptr = quotes_node.get();
          query_stack.back()->AddChild(std::move(quotes_node));
          query_stack.push_back(quotes_node_ptr);
          in_quotes = true;
        } else {
          query_stack.pop_back();  // Stop adding to the quoted phrase.
          in_quotes = false;
        }
      }
    }
  }

  root->RemoveEmptySubnodes();
  return true;
}

// static
void QueryParser::ExtractQueryWords(const std::u16string& text,
                                    QueryWordVector* words) {
  DCHECK(text == base::i18n::ToLower(text))
      << "The caller must have already lowercased `text`. Value = "
      << base::UTF16ToUTF8(text);
  base::i18n::BreakIterator iter(text, base::i18n::BreakIterator::BREAK_WORD);
  // TODO(evanm): support a locale here
  if (!iter.Init())
    return;

  while (iter.Advance()) {
    // Just found a span between 'prev' (inclusive) and 'pos' (exclusive). It
    // is not necessarily a word, but could also be a sequence of punctuation
    // or whitespace.
    if (iter.IsWord()) {
      std::u16string word = iter.GetString();
      if (!word.empty()) {
        words->push_back(QueryWord());
        words->back().word = word;
        words->back().position = iter.prev();
     }
    }
  }
}

// static
void QueryParser::SortAndCoalesceMatchPositions(
    Snippet::MatchPositions* matches) {
  std::sort(matches->begin(), matches->end(), &CompareMatchPosition);
  // WARNING: we don't use iterator here as CoalesceMatchesFrom may remove
  // from matches.
  for (size_t i = 0; i < matches->size(); ++i)
    CoalesceMatchesFrom(i, matches);
}

}  // namespace query_parser
