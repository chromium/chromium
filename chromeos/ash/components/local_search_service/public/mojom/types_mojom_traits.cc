// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/public/mojom/types_mojom_traits.h"

#include "mojo/public/cpp/base/string16_mojom_traits.h"

namespace mojo {

// static
ash::local_search_service::mojom::IndexId
EnumTraits<ash::local_search_service::mojom::IndexId,
           ash::local_search_service::IndexId>::
    ToMojom(ash::local_search_service::IndexId input) {
  switch (input) {
    case ash::local_search_service::IndexId::kCrosSettings:
      return ash::local_search_service::mojom::IndexId::kCrosSettings;
    case ash::local_search_service::IndexId::kHelpApp:
      return ash::local_search_service::mojom::IndexId::kHelpApp;
    case ash::local_search_service::IndexId::kHelpAppLauncher:
      return ash::local_search_service::mojom::IndexId::kHelpAppLauncher;
    case ash::local_search_service::IndexId::kPersonalization:
      return ash::local_search_service::mojom::IndexId::kPersonalization;
    case ash::local_search_service::IndexId::kShortcutsApp:
      return ash::local_search_service::mojom::IndexId::kShortcutsApp;
  }
  NOTREACHED_IN_MIGRATION();
  return ash::local_search_service::mojom::IndexId::kCrosSettings;
}

// static
bool EnumTraits<ash::local_search_service::mojom::IndexId,
                ash::local_search_service::IndexId>::
    FromMojom(ash::local_search_service::mojom::IndexId input,
              ash::local_search_service::IndexId* output) {
  switch (input) {
    case ash::local_search_service::mojom::IndexId::kCrosSettings:
      *output = ash::local_search_service::IndexId::kCrosSettings;
      return true;
    case ash::local_search_service::mojom::IndexId::kHelpApp:
      *output = ash::local_search_service::IndexId::kHelpApp;
      return true;
    case ash::local_search_service::mojom::IndexId::kHelpAppLauncher:
      *output = ash::local_search_service::IndexId::kHelpAppLauncher;
      return true;
    case ash::local_search_service::mojom::IndexId::kPersonalization:
      *output = ash::local_search_service::IndexId::kPersonalization;
      return true;
    case ash::local_search_service::mojom::IndexId::kShortcutsApp:
      *output = ash::local_search_service::IndexId::kShortcutsApp;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
ash::local_search_service::mojom::Backend
EnumTraits<ash::local_search_service::mojom::Backend,
           ash::local_search_service::Backend>::
    ToMojom(ash::local_search_service::Backend input) {
  switch (input) {
    case ash::local_search_service::Backend::kLinearMap:
      return ash::local_search_service::mojom::Backend::kLinearMap;
    case ash::local_search_service::Backend::kInvertedIndex:
      return ash::local_search_service::mojom::Backend::kInvertedIndex;
  }
  NOTREACHED_IN_MIGRATION();
  return ash::local_search_service::mojom::Backend::kLinearMap;
}

// static
bool EnumTraits<ash::local_search_service::mojom::Backend,
                ash::local_search_service::Backend>::
    FromMojom(ash::local_search_service::mojom::Backend input,
              ash::local_search_service::Backend* output) {
  switch (input) {
    case ash::local_search_service::mojom::Backend::kLinearMap:
      *output = ash::local_search_service::Backend::kLinearMap;
      return true;
    case ash::local_search_service::mojom::Backend::kInvertedIndex:
      *output = ash::local_search_service::Backend::kInvertedIndex;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

// static
bool StructTraits<ash::local_search_service::mojom::ContentDataView,
                  ash::local_search_service::Content>::
    Read(ash::local_search_service::mojom::ContentDataView data,
         ash::local_search_service::Content* out) {
  std::string id;
  std::u16string content;
  if (!data.ReadId(&id) || !data.ReadContent(&content))
    return false;

  *out = ash::local_search_service::Content(id, content, data.weight());
  return true;
}

// static
bool StructTraits<ash::local_search_service::mojom::DataDataView,
                  ash::local_search_service::Data>::
    Read(ash::local_search_service::mojom::DataDataView data,
         ash::local_search_service::Data* out) {
  std::string id;
  std::vector<ash::local_search_service::Content> contents;
  std::string locale;
  if (!data.ReadId(&id) || !data.ReadContents(&contents) ||
      !data.ReadLocale(&locale))
    return false;

  *out = ash::local_search_service::Data(id, contents, locale);
  return true;
}

// static
bool StructTraits<ash::local_search_service::mojom::SearchParamsDataView,
                  ash::local_search_service::SearchParams>::
    Read(ash::local_search_service::mojom::SearchParamsDataView data,
         ash::local_search_service::SearchParams* out) {
  *out = ash::local_search_service::SearchParams();
  out->relevance_threshold = data.relevance_threshold();
  out->prefix_threshold = data.prefix_threshold();
  out->fuzzy_threshold = data.fuzzy_threshold();
  return true;
}

// static
bool StructTraits<ash::local_search_service::mojom::PositionDataView,
                  ash::local_search_service::Position>::
    Read(ash::local_search_service::mojom::PositionDataView data,
         ash::local_search_service::Position* out) {
  *out = ash::local_search_service::Position();
  if (!data.ReadContentId(&(out->content_id)))
    return false;

  out->start = data.start();
  out->length = data.length();
  return true;
}

// static
bool StructTraits<ash::local_search_service::mojom::ResultDataView,
                  ash::local_search_service::Result>::
    Read(ash::local_search_service::mojom::ResultDataView data,
         ash::local_search_service::Result* out) {
  std::string id;
  std::vector<ash::local_search_service::Position> positions;
  if (!data.ReadId(&id) || !data.ReadPositions(&positions))
    return false;

  *out = ash::local_search_service::Result();
  out->id = id;
  out->score = data.score();
  out->positions = positions;
  return true;
}

// static
ash::local_search_service::mojom::ResponseStatus
EnumTraits<ash::local_search_service::mojom::ResponseStatus,
           ash::local_search_service::ResponseStatus>::
    ToMojom(ash::local_search_service::ResponseStatus input) {
  switch (input) {
    case ash::local_search_service::ResponseStatus::kUnknownError:
      return ash::local_search_service::mojom::ResponseStatus::kUnknownError;
    case ash::local_search_service::ResponseStatus::kSuccess:
      return ash::local_search_service::mojom::ResponseStatus::kSuccess;
    case ash::local_search_service::ResponseStatus::kEmptyQuery:
      return ash::local_search_service::mojom::ResponseStatus::kEmptyQuery;
    case ash::local_search_service::ResponseStatus::kEmptyIndex:
      return ash::local_search_service::mojom::ResponseStatus::kEmptyIndex;
  }
  NOTREACHED_IN_MIGRATION();
  return ash::local_search_service::mojom::ResponseStatus::kUnknownError;
}

// static
bool EnumTraits<ash::local_search_service::mojom::ResponseStatus,
                ash::local_search_service::ResponseStatus>::
    FromMojom(ash::local_search_service::mojom::ResponseStatus input,
              ash::local_search_service::ResponseStatus* output) {
  switch (input) {
    case ash::local_search_service::mojom::ResponseStatus::kUnknownError:
      *output = ash::local_search_service::ResponseStatus::kUnknownError;
      return true;
    case ash::local_search_service::mojom::ResponseStatus::kSuccess:
      *output = ash::local_search_service::ResponseStatus::kSuccess;
      return true;
    case ash::local_search_service::mojom::ResponseStatus::kEmptyQuery:
      *output = ash::local_search_service::ResponseStatus::kEmptyQuery;
      return true;
    case ash::local_search_service::mojom::ResponseStatus::kEmptyIndex:
      *output = ash::local_search_service::ResponseStatus::kEmptyIndex;
      return true;
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

}  // namespace mojo
