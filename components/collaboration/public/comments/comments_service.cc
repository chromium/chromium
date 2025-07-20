// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/public/comments/comments_service.h"

namespace collaboration::comments {

AttributionData::AttributionData() = default;
AttributionData::AttributionData(UrlAttribution url_data)
    : url_data_(std::move(url_data)), content_data_(std::nullopt) {}
AttributionData::AttributionData(UrlAttribution url_data,
                                 ContentAttribution content_data)
    : url_data_(std::move(url_data)), content_data_(std::move(content_data)) {}
AttributionData::~AttributionData() = default;
AttributionData::AttributionData(const AttributionData&) = default;
AttributionData& AttributionData::operator=(const AttributionData&) = default;
AttributionData::AttributionData(AttributionData&&) = default;
AttributionData& AttributionData::operator=(AttributionData&&) = default;

std::optional<UrlAttribution> AttributionData::GetAsUrl() const {
  if (url_data_.has_value()) {
    return url_data_;
  }
  return std::nullopt;
}

std::optional<ContentAttribution> AttributionData::GetAsContent() const {
  if (content_data_.has_value()) {
    return content_data_;
  }
  return std::nullopt;
}

AttributionLevel AttributionData::GetAttributionLevel() const {
  if (content_data_.has_value() && url_data_.has_value()) {
    return AttributionLevel::kContent;
  }
  if (url_data_.has_value()) {
    return AttributionLevel::kUrl;
  }
  return AttributionLevel::kNone;
}

Comment::Comment() = default;
Comment::~Comment() = default;
Comment::Comment(const Comment&) = default;
Comment& Comment::operator=(const Comment&) = default;
Comment::Comment(Comment&&) = default;
Comment& Comment::operator=(Comment&&) = default;

HierarchicalComment::HierarchicalComment() = default;
HierarchicalComment::~HierarchicalComment() = default;
HierarchicalComment::HierarchicalComment(const HierarchicalComment&) = default;
HierarchicalComment& HierarchicalComment::operator=(
    const HierarchicalComment&) = default;
HierarchicalComment::HierarchicalComment(HierarchicalComment&&) = default;
HierarchicalComment& HierarchicalComment::operator=(HierarchicalComment&&) =
    default;

FilterCriteria::FilterCriteria() = default;
FilterCriteria::~FilterCriteria() = default;
FilterCriteria::FilterCriteria(const FilterCriteria&) = default;
FilterCriteria& FilterCriteria::operator=(const FilterCriteria&) = default;
FilterCriteria::FilterCriteria(FilterCriteria&&) = default;
FilterCriteria& FilterCriteria::operator=(FilterCriteria&&) = default;

QueryResult::QueryResult() = default;
QueryResult::~QueryResult() = default;
QueryResult::QueryResult(const QueryResult&) = default;
QueryResult& QueryResult::operator=(const QueryResult&) = default;
QueryResult::QueryResult(QueryResult&&) = default;
QueryResult& QueryResult::operator=(QueryResult&&) = default;

}  // namespace collaboration::comments
