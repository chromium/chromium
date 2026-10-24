// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/shared_structs.h"

#include <optional>
#include <utility>

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/components/local_search_service/linear_map_search.h"
#include "chromeos/ash/components/string_matching/fuzzy_tokenized_string_match.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"

namespace ash::local_search_service {

local_search_service::Content::Content(const std::string& id,
                                       const std::u16string& content,
                                       double weight)
    : id(id), content(content), weight(weight) {}
local_search_service::Content::Content() = default;
local_search_service::Content::Content(const Content& content) = default;
local_search_service::Content::~Content() = default;

Data::Data(const std::string& id,
           const std::vector<Content>& contents,
           const std::string& locale)
    : id(id), contents(contents), locale(locale) {}
Data::Data() = default;
Data::Data(const Data& data) = default;
Data::~Data() = default;

Position::Position() = default;
Position::Position(const Position& position) = default;
Position::Position(const std::string& content_id,
                   uint32_t start,
                   uint32_t length)
    : content_id(content_id), start(start), length(length) {}
Position::~Position() = default;

Result::Result() = default;
Result::Result(const Result& result) = default;
Result::Result(const std::string& id,
               double score,
               const std::vector<Position>& positions)
    : id(id), score(score), positions(positions) {}
Result::~Result() = default;

WeightedPosition::WeightedPosition() = default;
WeightedPosition::WeightedPosition(const WeightedPosition& weighted_position) =
    default;
WeightedPosition::WeightedPosition(double weight, const Position& position)
    : weight(weight), position(position) {}
WeightedPosition::~WeightedPosition() = default;

Token::Token() = default;
Token::Token(const std::u16string& text,
             const std::vector<WeightedPosition>& pos)
    : content(text), positions(pos) {}
Token::Token(const Token& token)
    : content(token.content), positions(token.positions) {}
Token::~Token() = default;

}  // namespace ash::local_search_service
