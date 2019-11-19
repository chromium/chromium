// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_MOJOM_TYPEMAPS_PUP_MOJOM_TRAITS_H_
#define CHROME_CHROME_CLEANER_MOJOM_TYPEMAPS_PUP_MOJOM_TRAITS_H_

#include <stdint.h>

#include "chrome/chrome_cleaner/mojom/pup.mojom.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"
#include "mojo/public/cpp/bindings/array_traits_stl.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<chrome_cleaner::mojom::TraceLocationDataView,
                    chrome_cleaner::UwS::TraceLocation> {
  static int32_t value(const chrome_cleaner::UwS::TraceLocation& location);

  static bool Read(chrome_cleaner::mojom::TraceLocationDataView view,
                   chrome_cleaner::UwS::TraceLocation* out);
};

template <>
struct StructTraits<chrome_cleaner::mojom::FileInfoDataView,
                    chrome_cleaner::PUPData::FileInfo> {
  static const std::set<chrome_cleaner::UwS::TraceLocation>& found_in(
      const chrome_cleaner::PUPData::FileInfo& info);

  static bool Read(chrome_cleaner::mojom::FileInfoDataView view,
                   chrome_cleaner::PUPData::FileInfo* out);
};

template <>
struct StructTraits<chrome_cleaner::mojom::PUPDataView,
                    chrome_cleaner::PUPData::PUP> {
  // It's safe to return a reference, since the PUP object outlives the Mojo
  // struct object.
  static const chrome_cleaner::UnorderedFilePathSet& expanded_disk_footprints(
      const chrome_cleaner::PUPData::PUP& pup);

  static const chrome_cleaner::PUPData::PUP::FileInfoMap::MapType&
  disk_footprints_info(const chrome_cleaner::PUPData::PUP& pup);

  static bool Read(chrome_cleaner::mojom::PUPDataView view,
                   chrome_cleaner::PUPData::PUP* out);
};

}  // namespace mojo

#endif  // CHROME_CHROME_CLEANER_MOJOM_TYPEMAPS_PUP_MOJOM_TRAITS_H_
