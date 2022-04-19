// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/public/mojom/types_mojom_traits.h"

#include "mojo/public/cpp/base/string16_mojom_traits.h"

namespace mojo {

// static
chromeos::local_search_service::mojom::IndexId
EnumTraits<chromeos::local_search_service::mojom::IndexId,
           chromeos::local_search_service::IndexId>::
    ToMojom(chromeos::local_search_service::IndexId input) {
  switch (input) {
    case chromeos::local_search_service::IndexId::kCrosSettings:
      return chromeos::local_search_service::mojom::IndexId::kCrosSettings;
    case chromeos::local_search_service::IndexId::kHelpApp:
      return chromeos::local_search_service::mojom::IndexId::kHelpApp;
    case chromeos::local_search_service::IndexId::kHelpAppLauncher:
      return chromeos::local_search_service::mojom::IndexId::kHelpAppLauncher;
    case chromeos::local_search_service::IndexId::kPersonalization:
      return chromeos::local_search_service::mojom::IndexId::kPersonalization;
  }
  NOTREACHED();
  return chromeos::local_search_service::mojom::IndexId::kCrosSettings;
}

// static
bool EnumTraits<chromeos::local_search_service::mojom::IndexId,
                chromeos::local_search_service::IndexId>::
    FromMojom(chromeos::local_search_service::mojom::IndexId input,
              chromeos::local_search_service::IndexId* output) {
  switch (input) {
    case chromeos::local_search_service::mojom::IndexId::kCrosSettings:
      *output = chromeos::local_search_service::IndexId::kCrosSettings;
      return true;
    case chromeos::local_search_service::mojom::IndexId::kHelpApp:
      *output = chromeos::local_search_service::IndexId::kHelpApp;
      return true;
    case chromeos::local_search_service::mojom::IndexId::kHelpAppLauncher:
      *output = chromeos::local_search_service::IndexId::kHelpAppLauncher;
      return true;
    case chromeos::local_search_service::mojom::IndexId::kPersonalization:
      *output = chromeos::local_search_service::IndexId::kPersonalization;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
chromeos::local_search_service::mojom::Backend
EnumTraits<chromeos::local_search_service::mojom::Backend,
           chromeos::local_search_service::Backend>::
    ToMojom(chromeos::local_search_service::Backend input) {
  switch (input) {
    case chromeos::local_search_service::Backend::kLinearMap:
      return chromeos::local_search_service::mojom::Backend::kLinearMap;
    case chromeos::local_search_service::Backend::kInvertedIndex:
      return chromeos::local_search_service::mojom::Backend::kInvertedIndex;
  }
  NOTREACHED();
  return chromeos::local_search_service::mojom::Backend::kLinearMap;
}

// static
bool EnumTraits<chromeos::local_search_service::mojom::Backend,
                chromeos::local_search_service::Backend>::
    FromMojom(chromeos::local_search_service::mojom::Backend input,
              chromeos::local_search_service::Backend* output) {
  switch (input) {
    case chromeos::local_search_service::mojom::Backend::kLinearMap:
      *output = chromeos::local_search_service::Backend::kLinearMap;
      return true;
    case chromeos::local_search_service::mojom::Backend::kInvertedIndex:
      *output = chromeos::local_search_service::Backend::kInvertedIndex;
      return true;
  }
  NOTREACHED();
  return false;
}

// static
bool StructTraits<chromeos::local_search_service::mojom::ContentDataView,
                  chromeos::local_search_service::Content>::
    Read(chromeos::local_search_service::mojom::ContentDataView data,
         chromeos::local_search_service::Content* out) {
  std::string id;
  std::u16string content;
  if (!data.ReadId(&id) || !data.ReadContent(&content))
    return false;

  *out = chromeos::local_search_service::Content(id, content, data.weight());
  return true;
}

// static
bool StructTraits<chromeos::local_search_service::mojom::DataDataView,
                  chromeos::local_search_service::Data>::
    Read(chromeos::local_search_service::mojom::DataDataView data,
         chromeos::local_search_service::Data* out) {
  std::string id;
  std::vector<chromeos::local_search_service::Content> contents;
  std::string locale;
  if (!data.ReadId(&id) || !data.ReadContents(&contents) ||
      !data.ReadLocale(&locale))
    return false;

  *out = chromeos::local_search_service::Data(id, contents, locale);
  return true;
}

// static
bool StructTraits<chromeos::local_search_service::mojom::SearchParamsDataView,
                  chromeos::local_search_service::SearchParams>::
    Read(chromeos::local_search_service::mojom::SearchParamsDataView data,
         chromeos::local_search_service::SearchParams* out) {
  *out = chromeos::local_search_service::SearchParams();
  out->relevance_threshold = data.relevance_threshold();
  out->prefix_threshold = data.prefix_threshold();
  out->fuzzy_threshold = data.fuzzy_threshold();
  return true;
}

// static
bool StructTraits<chromeos::local_search_service::mojom::PositionDataView,
                  chromeos::local_search_service::Position>::
    Read(chromeos::local_search_service::mojom::PositionDataView data,
         chromeos::local_search_service::Position* out) {
  *out = chromeos::local_search_service::Position();
  if (!data.ReadContentId(&(out->content_id)))
    return false;

  out->start = data.start();
  out->length = data.length();
  return true;
}

// static
bool StructTraits<chromeos::local_search_service::mojom::ResultDataView,
                  chromeos::local_search_service::Result>::
    Read(chromeos::local_search_service::mojom::ResultDataView data,
         chromeos::local_search_service::Result* out) {
  std::string id;
  std::vector<chromeos::local_search_service::Position> positions;
  if (!data.ReadId(&id) || !data.ReadPositions(&positions))
    return false;

  *out = chromeos::local_search_service::Result();
  out->id = id;
  out->score = data.score();
  out->positions = positions;
  return true;
}

// static
chromeos::local_search_service::mojom::ResponseStatus
EnumTraits<chromeos::local_search_service::mojom::ResponseStatus,
           chromeos::local_search_service::ResponseStatus>::
    ToMojom(chromeos::local_search_service::ResponseStatus input) {
  switch (input) {
    case chromeos::local_search_service::ResponseStatus::kUnknownError:
      return chromeos::local_search_service::mojom::ResponseStatus::
          kUnknownError;
    case chromeos::local_search_service::ResponseStatus::kSuccess:
      return chromeos::local_search_service::mojom::ResponseStatus::kSuccess;
    case chromeos::local_search_service::ResponseStatus::kEmptyQuery:
      return chromeos::local_search_service::mojom::ResponseStatus::kEmptyQuery;
    case chromeos::local_search_service::ResponseStatus::kEmptyIndex:
      return chromeos::local_search_service::mojom::ResponseStatus::kEmptyIndex;
  }
  NOTREACHED();
  return chromeos::local_search_service::mojom::ResponseStatus::kUnknownError;
}

// static
bool EnumTraits<chromeos::local_search_service::mojom::ResponseStatus,
                chromeos::local_search_service::ResponseStatus>::
    FromMojom(chromeos::local_search_service::mojom::ResponseStatus input,
              chromeos::local_search_service::ResponseStatus* output) {
  switch (input) {
    case chromeos::local_search_service::mojom::ResponseStatus::kUnknownError:
      *output = chromeos::local_search_service::ResponseStatus::kUnknownError;
      return true;
    case chromeos::local_search_service::mojom::ResponseStatus::kSuccess:
      *output = chromeos::local_search_service::ResponseStatus::kSuccess;
      return true;
    case chromeos::local_search_service::mojom::ResponseStatus::kEmptyQuery:
      *output = chromeos::local_search_service::ResponseStatus::kEmptyQuery;
      return true;
    case chromeos::local_search_service::mojom::ResponseStatus::kEmptyIndex:
      *output = chromeos::local_search_service::ResponseStatus::kEmptyIndex;
      return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace mojo
