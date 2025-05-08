// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TAB_GROUPS_TOKEN_ID_H_
#define COMPONENTS_TAB_GROUPS_TOKEN_ID_H_

#include <ostream>

#include "base/component_export.h"
#include "base/token.h"

namespace tab_groups {

// A class template for token based ids. For new use cases, instantiate a new
// class from this template using itself as the argument (CRTP), so that the ids
// are not comparable to other use cases' ids. See tab_groups::TabGroupId for an
// example of this.
template <class T>
class COMPONENT_EXPORT(TAB_GROUPS) TokenId {
 public:
  static T GenerateNew() { return T(TokenId(base::Token::CreateRandom())); }

  // This should only called with |token| returned from a previous |token()|
  // call on a valid TokenId.
  static T FromRawToken(base::Token token) { return T(TokenId(token)); }

  // Should only be used if intending to populate the TokenId by reference,
  // using a valid existing ID. Primarily needed for the extensions API.
  static T CreateEmpty() { return T(TokenId(base::Token())); }

  TokenId(const TokenId<T>& other) = default;

  TokenId<T>& operator=(const TokenId<T>& other) = default;

  friend bool operator==(const TokenId<T>&, const TokenId<T>&) = default;
  friend auto operator<=>(const TokenId<T>&, const TokenId<T>&) = default;

  const base::Token& token() const { return token_; }

  bool is_empty() const { return token_.is_zero(); }

  std::string ToString() const { return token_.ToString(); }

 private:
  explicit TokenId(base::Token token) : token_(token) {}

  base::Token token_;
};

// For use in std::unordered_map.
template <class T>
struct COMPONENT_EXPORT(TAB_GROUPS) TokenIdHash {
 public:
  size_t operator()(const TokenId<T>& token_id) const {
    return base::TokenHash()(token_id.token());
  }
};

// Stream operator so TokenId objects can be used in logging statements.
template <class T>
std::ostream& operator<<(std::ostream& out, const TokenId<T>& token_id) {
  return out << token_id.ToString();
}

}  // namespace tab_groups

#endif  // COMPONENTS_TAB_GROUPS_TOKEN_ID_H_
