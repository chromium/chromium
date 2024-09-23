// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PRIVACY_BUDGET_FIELD_TRIAL_PARAM_CONVERSIONS_H_
#define CHROME_COMMON_PRIVACY_BUDGET_FIELD_TRIAL_PARAM_CONVERSIONS_H_

#include <iterator>
#include <string_view>
#include <type_traits>
#include <vector>

#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"

// The Encode/Decode families of functions are meant to be used to encode
// various types so that they can be specified via field trial configurations
// and Prefs.

namespace privacy_budget_internal {

// DecodeIdentifiabilityType() overloads should not be called directly. Instead
// use either DecodeIdentifiabilityFieldTrialParam() or
// EncodeIdentifiabilityFieldTrialParam().
//
// DecodeIdentifiabilityType(std::string_view s,V* v) decodes a element of type
// V serialized as a string and referred to via std::string_view s and stores it
// in *v.
bool DecodeIdentifiabilityType(const std::string_view,
                               blink::IdentifiableSurface*);
bool DecodeIdentifiabilityType(const std::string_view,
                               blink::IdentifiableSurface::Type*);
bool DecodeIdentifiabilityType(const std::string_view, int*);
bool DecodeIdentifiabilityType(const std::string_view, uint64_t*);
bool DecodeIdentifiabilityType(const std::string_view, unsigned int*);
bool DecodeIdentifiabilityType(const std::string_view, double*);
bool DecodeIdentifiabilityType(const std::string_view,
                               std::vector<blink::IdentifiableSurface>*);
bool DecodeIdentifiabilityType(const std::string_view, std::string*);

// V is a std::pair<P,R> where P and R are types known to
// DecodeIdentifiabilityType().
template <
    typename V,
    typename P = typename std::remove_const<typename V::first_type>::type,
    typename R = typename std::remove_const<typename V::second_type>::type>
bool DecodeIdentifiabilityType(std::string_view s, V* result) {
  auto pieces =
      base::SplitStringPiece(s, ";", base::WhitespaceHandling::TRIM_WHITESPACE,
                             base::SplitResult::SPLIT_WANT_NONEMPTY);
  if (pieces.size() != 2)
    return false;
  P first;
  R second;
  if (!DecodeIdentifiabilityType(pieces[0], &first) ||
      !DecodeIdentifiabilityType(pieces[1], &second))
    return false;
  ::new (result) V(std::move(first), std::move(second));
  return true;
}

std::string EncodeIdentifiabilityType(const blink::IdentifiableSurface&);
std::string EncodeIdentifiabilityType(const blink::IdentifiableSurface::Type&);
std::string EncodeIdentifiabilityType(const unsigned int&);
std::string EncodeIdentifiabilityType(const double&);
std::string EncodeIdentifiabilityType(const uint64_t&);
std::string EncodeIdentifiabilityType(const int&);
template <typename T, typename U>
std::string EncodeIdentifiabilityType(const std::pair<T, U>& v) {
  return base::StrCat({EncodeIdentifiabilityType(v.first), ";",
                       base::NumberToString(v.second)});
}
std::string EncodeIdentifiabilityType(
    const std::vector<blink::IdentifiableSurface>& value);
std::string EncodeIdentifiabilityType(const std::string& value);

template <typename T>
struct NoOpFilter {
  bool operator()(T t) { return true; }
};

// Instantiate with a type and inherit from std::true_type in order to sort the
// encoded elements of a container. The ordering is undefined but stable across
// versions of Chrome.
template <typename T>
struct SortWhenSerializing : std::false_type {};

}  // namespace privacy_budget_internal

// Decodes a field trial parameter containing a list of values. The result is
// returned in the form of a container type that must be specified at
// instantiation. There should be a valid DecodeIdentifiabilityType
// specialization for the container's value type.
template <typename T,
          char Separator = ',',
          typename V = typename T::value_type,
          bool ElementDecoder(const std::string_view, V*) =
              &privacy_budget_internal::DecodeIdentifiabilityType>
T DecodeIdentifiabilityFieldTrialParam(std::string_view encoded_value) {
  T result;
  auto pieces =
      base::SplitStringPiece(encoded_value, std::string(1, Separator),
                             base::WhitespaceHandling::TRIM_WHITESPACE,
                             base::SplitResult::SPLIT_WANT_NONEMPTY);
  auto inserter = std::inserter(result, result.end());
  for (const auto& piece : pieces) {
    V v;
    if (!ElementDecoder(piece, &v))
      continue;
    inserter = v;
  }
  return result;
}

std::string EncodeIdentifiabilityFieldTrialParam(bool source);

// Encodes a field trial parameter that will contain a list of values taken from
// a container. The container must satisfy the named requirement Container. Its
// value_type must have a corresponding EncodeIdentifiabilityType
// specialization.
template <typename T,
          std::string ElementEncoder(const typename T::value_type&) =
              privacy_budget_internal::EncodeIdentifiabilityType>
std::string EncodeIdentifiabilityFieldTrialParam(const T& source) {
  std::vector<std::string> encoded_elements;
  encoded_elements.reserve(source.size());
  for (const auto& v : source) {
    encoded_elements.emplace_back(ElementEncoder(v));
  }
  if (privacy_budget_internal::SortWhenSerializing<
          typename std::remove_cv<T>::type>::value) {
    base::ranges::sort(encoded_elements);
  }
  return base::JoinString(encoded_elements, ",");
}

#endif  // CHROME_COMMON_PRIVACY_BUDGET_FIELD_TRIAL_PARAM_CONVERSIONS_H_
