// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_PUBLIC_MOJOM_TYPES_MOJOM_TRAITS_H_
#define CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_PUBLIC_MOJOM_TYPES_MOJOM_TRAITS_H_

#include <string>

#include "chromeos/ash/components/local_search_service/public/mojom/local_search_service.mojom-shared.h"
#include "chromeos/ash/components/local_search_service/public/mojom/types.mojom-shared.h"
#include "chromeos/ash/components/local_search_service/shared_structs.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<ash::local_search_service::mojom::IndexId,
                  ash::local_search_service::IndexId> {
  static ash::local_search_service::mojom::IndexId ToMojom(
      ash::local_search_service::IndexId input);
  static bool FromMojom(ash::local_search_service::mojom::IndexId input,
                        ash::local_search_service::IndexId* output);
};

template <>
struct EnumTraits<ash::local_search_service::mojom::Backend,
                  ash::local_search_service::Backend> {
  static ash::local_search_service::mojom::Backend ToMojom(
      ash::local_search_service::Backend input);
  static bool FromMojom(ash::local_search_service::mojom::Backend input,
                        ash::local_search_service::Backend* output);
};

template <>
struct StructTraits<ash::local_search_service::mojom::ContentDataView,
                    ash::local_search_service::Content> {
 public:
  static std::string id(const ash::local_search_service::Content& c) {
    return c.id;
  }
  static std::u16string content(const ash::local_search_service::Content& c) {
    return c.content;
  }
  static double weight(const ash::local_search_service::Content& c) {
    return c.weight;
  }

  static bool Read(ash::local_search_service::mojom::ContentDataView data,
                   ash::local_search_service::Content* out);
};

template <>
struct StructTraits<ash::local_search_service::mojom::DataDataView,
                    ash::local_search_service::Data> {
 public:
  static std::string id(const ash::local_search_service::Data& d) {
    return d.id;
  }
  static std::vector<ash::local_search_service::Content> contents(
      const ash::local_search_service::Data& d) {
    return d.contents;
  }

  static std::string locale(const ash::local_search_service::Data& d) {
    return d.locale;
  }

  static bool Read(ash::local_search_service::mojom::DataDataView data,
                   ash::local_search_service::Data* out);
};

template <>
struct StructTraits<ash::local_search_service::mojom::SearchParamsDataView,
                    ash::local_search_service::SearchParams> {
 public:
  static double relevance_threshold(
      const ash::local_search_service::SearchParams& s) {
    return s.relevance_threshold;
  }
  static double prefix_threshold(
      const ash::local_search_service::SearchParams& s) {
    return s.prefix_threshold;
  }
  static double fuzzy_threshold(
      const ash::local_search_service::SearchParams& s) {
    return s.fuzzy_threshold;
  }

  static bool Read(ash::local_search_service::mojom::SearchParamsDataView data,
                   ash::local_search_service::SearchParams* out);
};

template <>
struct StructTraits<ash::local_search_service::mojom::PositionDataView,
                    ash::local_search_service::Position> {
 public:
  static std::string content_id(const ash::local_search_service::Position& p) {
    return p.content_id;
  }
  static uint32_t start(const ash::local_search_service::Position& p) {
    return p.start;
  }
  static uint32_t length(const ash::local_search_service::Position& p) {
    return p.length;
  }

  static bool Read(ash::local_search_service::mojom::PositionDataView data,
                   ash::local_search_service::Position* out);
};

template <>
struct StructTraits<ash::local_search_service::mojom::ResultDataView,
                    ash::local_search_service::Result> {
 public:
  static std::string id(const ash::local_search_service::Result& r) {
    return r.id;
  }
  static double score(const ash::local_search_service::Result& r) {
    return r.score;
  }
  static std::vector<ash::local_search_service::Position> positions(
      const ash::local_search_service::Result& r) {
    return r.positions;
  }

  static bool Read(ash::local_search_service::mojom::ResultDataView data,
                   ash::local_search_service::Result* out);
};

template <>
struct EnumTraits<ash::local_search_service::mojom::ResponseStatus,
                  ash::local_search_service::ResponseStatus> {
  static ash::local_search_service::mojom::ResponseStatus ToMojom(
      ash::local_search_service::ResponseStatus input);
  static bool FromMojom(ash::local_search_service::mojom::ResponseStatus input,
                        ash::local_search_service::ResponseStatus* out);
};

}  // namespace mojo

#endif  // CHROMEOS_ASH_COMPONENTS_LOCAL_SEARCH_SERVICE_PUBLIC_MOJOM_TYPES_MOJOM_TRAITS_H_
