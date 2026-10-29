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

local_search_service::Content::Content(std::string id,
                                       std::u16string content,
                                       double weight)
    : id(std::move(id)), content(std::move(content)), weight(weight) {}
local_search_service::Content::Content() = default;
local_search_service::Content::Content(const Content& other) = default;
local_search_service::Content::Content(Content&& other) = default;
Content& local_search_service::Content::operator=(const Content& other) =
    default;
Content& local_search_service::Content::operator=(Content&& other) = default;
local_search_service::Content::~Content() = default;

Data::Data(std::string id,
           std::vector<Content> contents,
           std::optional<base::i18n::LanguageTag> locale)
    : id(std::move(id)),
      contents(std::move(contents)),
      locale(std::move(locale)) {}
Data::Data() = default;
Data::Data(const Data& data) = default;
Data::Data(Data&& data) = default;
Data& Data::operator=(const Data& data) = default;
Data& Data::operator=(Data&& data) = default;
Data::~Data() = default;

Position::Position() = default;
Position::Position(std::string content_id, uint32_t start, uint32_t length)
    : content_id(std::move(content_id)), start(start), length(length) {}
Position::Position(const Position& position) = default;
Position::Position(Position&& position) = default;
Position& Position::operator=(const Position& position) = default;
Position& Position::operator=(Position&& position) = default;
Position::~Position() = default;

Result::Result() = default;
Result::Result(std::string id, double score, std::vector<Position> positions)
    : id(std::move(id)), score(score), positions(std::move(positions)) {}
Result::Result(const Result& result) = default;
Result::Result(Result&& result) = default;
Result& Result::operator=(const Result& result) = default;
Result& Result::operator=(Result&& result) = default;
Result::~Result() = default;

WeightedPosition::WeightedPosition() = default;
WeightedPosition::WeightedPosition(double weight, Position position)
    : weight(weight), position(std::move(position)) {}
WeightedPosition::WeightedPosition(const WeightedPosition& weighted_position) =
    default;
WeightedPosition::WeightedPosition(WeightedPosition&& weighted_position) =
    default;
WeightedPosition& WeightedPosition::operator=(
    const WeightedPosition& weighted_position) = default;
WeightedPosition& WeightedPosition::operator=(
    WeightedPosition&& weighted_position) = default;
WeightedPosition::~WeightedPosition() = default;

Token::Token() = default;
Token::Token(std::u16string text, std::vector<WeightedPosition> pos)
    : content(std::move(text)), positions(std::move(pos)) {}
Token::Token(const Token& token) = default;
Token::Token(Token&& token) = default;
Token& Token::operator=(const Token& token) = default;
Token& Token::operator=(Token&& token) = default;
Token::~Token() = default;

}  // namespace ash::local_search_service
