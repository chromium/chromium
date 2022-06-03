// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_PUBLIC_MOJOM_TYPES_MOJOM_TRAITS_H_
#define CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_PUBLIC_MOJOM_TYPES_MOJOM_TRAITS_H_

#include <string>

#include "chromeos/components/local_search_service/public/mojom/local_search_service.mojom-shared.h"
#include "chromeos/components/local_search_service/public/mojom/types.mojom-shared.h"
#include "chromeos/components/local_search_service/shared_structs.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<chromeos::local_search_service::mojom::IndexId,
                  chromeos::local_search_service::IndexId> {
  static chromeos::local_search_service::mojom::IndexId ToMojom(
      chromeos::local_search_service::IndexId input);
  static bool FromMojom(chromeos::local_search_service::mojom::IndexId input,
                        chromeos::local_search_service::IndexId* output);
};

template <>
struct EnumTraits<chromeos::local_search_service::mojom::Backend,
                  chromeos::local_search_service::Backend> {
  static chromeos::local_search_service::mojom::Backend ToMojom(
      chromeos::local_search_service::Backend input);
  static bool FromMojom(chromeos::local_search_service::mojom::Backend input,
                        chromeos::local_search_service::Backend* output);
};

template <>
struct StructTraits<chromeos::local_search_service::mojom::ContentDataView,
                    chromeos::local_search_service::Content> {
 public:
  static std::string id(const chromeos::local_search_service::Content& c) {
    return c.id;
  }
  static std::u16string content(
      const chromeos::local_search_service::Content& c) {
    return c.content;
  }
  static double weight(const chromeos::local_search_service::Content& c) {
    return c.weight;
  }

  static bool Read(chromeos::local_search_service::mojom::ContentDataView data,
                   chromeos::local_search_service::Content* out);
};

template <>
struct StructTraits<chromeos::local_search_service::mojom::DataDataView,
                    chromeos::local_search_service::Data> {
 public:
  static std::string id(const chromeos::local_search_service::Data& d) {
    return d.id;
  }
  static std::vector<chromeos::local_search_service::Content> contents(
      const chromeos::local_search_service::Data& d) {
    return d.contents;
  }

  static std::string locale(const chromeos::local_search_service::Data& d) {
    return d.locale;
  }

  static bool Read(chromeos::local_search_service::mojom::DataDataView data,
                   chromeos::local_search_service::Data* out);
};

template <>
struct StructTraits<chromeos::local_search_service::mojom::SearchParamsDataView,
                    chromeos::local_search_service::SearchParams> {
 public:
  static double relevance_threshold(
      const chromeos::local_search_service::SearchParams& s) {
    return s.relevance_threshold;
  }
  static double prefix_threshold(
      const chromeos::local_search_service::SearchParams& s) {
    return s.prefix_threshold;
  }
  static double fuzzy_threshold(
      const chromeos::local_search_service::SearchParams& s) {
    return s.fuzzy_threshold;
  }

  static bool Read(
      chromeos::local_search_service::mojom::SearchParamsDataView data,
      chromeos::local_search_service::SearchParams* out);
};

template <>
struct StructTraits<chromeos::local_search_service::mojom::PositionDataView,
                    chromeos::local_search_service::Position> {
 public:
  static std::string content_id(
      const chromeos::local_search_service::Position& p) {
    return p.content_id;
  }
  static uint32_t start(const chromeos::local_search_service::Position& p) {
    return p.start;
  }
  static uint32_t length(const chromeos::local_search_service::Position& p) {
    return p.length;
  }

  static bool Read(chromeos::local_search_service::mojom::PositionDataView data,
                   chromeos::local_search_service::Position* out);
};

template <>
struct StructTraits<chromeos::local_search_service::mojom::ResultDataView,
                    chromeos::local_search_service::Result> {
 public:
  static std::string id(const chromeos::local_search_service::Result& r) {
    return r.id;
  }
  static double score(const chromeos::local_search_service::Result& r) {
    return r.score;
  }
  static std::vector<chromeos::local_search_service::Position> positions(
      const chromeos::local_search_service::Result& r) {
    return r.positions;
  }

  static bool Read(chromeos::local_search_service::mojom::ResultDataView data,
                   chromeos::local_search_service::Result* out);
};

template <>
struct EnumTraits<chromeos::local_search_service::mojom::ResponseStatus,
                  chromeos::local_search_service::ResponseStatus> {
  static chromeos::local_search_service::mojom::ResponseStatus ToMojom(
      chromeos::local_search_service::ResponseStatus input);
  static bool FromMojom(
      chromeos::local_search_service::mojom::ResponseStatus input,
      chromeos::local_search_service::ResponseStatus* out);
};

}  // namespace mojo

#endif  // CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_PUBLIC_MOJOM_TYPES_MOJOM_TRAITS_H_
