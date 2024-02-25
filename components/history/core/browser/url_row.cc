// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/url_row.h"

#include <algorithm>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"

namespace history {

URLRow::URLRow() = default;

URLRow::URLRow(const GURL& url) : url_(url) {}

URLRow::URLRow(const GURL& url, URLID id) : id_(id), url_(url) {}

URLRow::URLRow(const URLRow& other) = default;

URLRow::URLRow(URLRow&& other) noexcept = default;

URLRow::~URLRow() = default;

URLRow& URLRow::operator=(const URLRow& other) = default;
URLRow& URLRow::operator=(URLRow&& other) noexcept = default;

void URLRow::Swap(URLRow* other) {
  std::swap(id_, other->id_);
  url_.Swap(&other->url_);
  title_.swap(other->title_);
  std::swap(visit_count_, other->visit_count_);
  std::swap(typed_count_, other->typed_count_);
  std::swap(last_visit_, other->last_visit_);
  std::swap(hidden_, other->hidden_);
}

size_t URLRow::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(url_) +
         base::trace_event::EstimateMemoryUsage(title_);
}

// Annotations
// ----------------------------------------------------------
VisitContentModelAnnotations::Category::Category(const std::string& id,
                                                 int weight)
    : id(id), weight(weight) {}
VisitContentModelAnnotations::Category::Category() = default;

// static
std::optional<VisitContentModelAnnotations::Category>
VisitContentModelAnnotations::Category::FromStringVector(
    const std::vector<std::string>& vector) {
  if (vector.size() != 2)
    return std::nullopt;

  VisitContentModelAnnotations::Category category;
  category.id = vector[0];
  if (!base::StringToInt(vector[1], &category.weight))
    return std::nullopt;
  return category;
}

std::string VisitContentModelAnnotations::Category::ToString() const {
  return base::StrCat({id, ":", base::NumberToString(weight)});
}

bool VisitContentModelAnnotations::Category::operator==(
    const VisitContentModelAnnotations::Category& other) const {
  return id == other.id && weight == other.weight;
}

bool VisitContentModelAnnotations::Category::operator!=(
    const VisitContentModelAnnotations::Category& other) const {
  return !(*this == other);
}

VisitContentModelAnnotations::VisitContentModelAnnotations(
    float visibility_score,
    const std::vector<Category>& categories,
    int64_t page_topics_model_version,
    const std::vector<Category>& entities)
    : visibility_score(visibility_score),
      categories(categories),
      page_topics_model_version(page_topics_model_version),
      entities(entities) {}
VisitContentModelAnnotations::VisitContentModelAnnotations() = default;
VisitContentModelAnnotations::VisitContentModelAnnotations(
    const VisitContentModelAnnotations&) = default;
VisitContentModelAnnotations::~VisitContentModelAnnotations() = default;

// static
void VisitContentModelAnnotations::MergeCategoryIntoVector(
    const Category& category,
    std::vector<Category>* categories) {
  DCHECK(categories);
  for (auto& this_category : *categories) {
    // If this visit already has the category, upgrade the weight.
    if (category.id == this_category.id) {
      this_category.weight = std::max(this_category.weight, category.weight);
      return;
    }
  }

  // Append the category since it wasn't found in our existing `categories`.
  categories->push_back(category);
}

void VisitContentModelAnnotations::MergeFrom(
    const VisitContentModelAnnotations& other) {
  // To be conservative, we use the lesser of the two visibility scores, but
  // ignore sentinel values (which are negative).
  if ((this->visibility_score < 0 && other.visibility_score >= 0) ||
      (this->visibility_score >= 0 && other.visibility_score >= 0 &&
       other.visibility_score < this->visibility_score)) {
    this->visibility_score = other.visibility_score;
  }

  for (auto& other_category : other.categories) {
    MergeCategoryIntoVector(other_category, &categories);
  }
  for (auto& other_entity : other.entities) {
    MergeCategoryIntoVector(other_entity, &entities);
  }
}

VisitContentAnnotations::VisitContentAnnotations(
    VisitContentAnnotationFlags annotation_flags,
    VisitContentModelAnnotations model_annotations,
    const std::vector<std::string>& related_searches,
    const GURL& search_normalized_url,
    const std::u16string& search_terms,
    const std::string& alternative_title,
    const std::string& page_language,
    PasswordState password_state,
    bool has_url_keyed_image)
    : annotation_flags(annotation_flags),
      model_annotations(model_annotations),
      related_searches(related_searches),
      search_normalized_url(search_normalized_url),
      search_terms(search_terms),
      alternative_title(alternative_title),
      page_language(page_language),
      password_state(password_state),
      has_url_keyed_image(has_url_keyed_image) {}
VisitContentAnnotations::VisitContentAnnotations() = default;
VisitContentAnnotations::VisitContentAnnotations(
    const VisitContentAnnotations&) = default;
VisitContentAnnotations::~VisitContentAnnotations() = default;

URLResult::URLResult() = default;

URLResult::URLResult(const GURL& url, base::Time visit_time)
    : URLRow(url), visit_time_(visit_time) {}

URLResult::URLResult(const URLRow& url_row) : URLRow(url_row) {}

URLResult::URLResult(const URLResult& other) = default;

// TODO(bug 706963) this should be implemented as "= default" when Android
// toolchain is updated.
URLResult::URLResult(URLResult&& other) noexcept
    : URLRow(std::move(other)),
      visit_time_(other.visit_time_),
      content_annotations_(other.content_annotations_),
      snippet_(std::move(other.snippet_)),
      title_match_positions_(std::move(other.title_match_positions_)),
      blocked_visit_(other.blocked_visit_),
      app_id_(std::move(other.app_id_)) {}

URLResult::~URLResult() = default;

URLResult& URLResult::operator=(const URLResult&) = default;

void URLResult::SwapResult(URLResult* other) {
  URLRow::Swap(other);
  std::swap(visit_time_, other->visit_time_);
  std::swap(content_annotations_, other->content_annotations_);
  snippet_.Swap(&other->snippet_);
  title_match_positions_.swap(other->title_match_positions_);
  std::swap(blocked_visit_, other->blocked_visit_);
  std::swap(app_id_, other->app_id_);
}

// static
bool URLResult::CompareVisitTime(const URLResult& lhs, const URLResult& rhs) {
  return lhs.visit_time() > rhs.visit_time();
}

}  // namespace history
