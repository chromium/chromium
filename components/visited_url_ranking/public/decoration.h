// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITED_URL_RANKING_PUBLIC_DECORATION_H_
#define COMPONENTS_VISITED_URL_RANKING_PUBLIC_DECORATION_H_

#include <string>

namespace visited_url_ranking {

// The currently supported Decorators that may be added to a URL Visit
// Aggregate
enum class DecorationType {
  kUnknown = 0,
  kVisitedXAgo = 1,
  kMostRecent = 2,
  kFrequentlyVisited = 3,
  kFrequentlyVisitedAtTime = 4,
};

// Holds the data for one of the decorations for a URL Visit Aggregate.
class Decoration {
 public:
  explicit Decoration(DecorationType decoration_type,
                      std::u16string display_string);
  Decoration(const Decoration&);
  virtual ~Decoration() = default;

  DecorationType GetType() const { return type_; }
  std::u16string GetDisplayString() const { return display_string_; }

 private:
  DecorationType type_;
  std::u16string display_string_;
};

}  // namespace visited_url_ranking

#endif  // COMPONENTS_VISITED_URL_RANKING_PUBLIC_DECORATION_H_
