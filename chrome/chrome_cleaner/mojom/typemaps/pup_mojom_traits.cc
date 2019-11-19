// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/mojom/typemaps/pup_mojom_traits.h"

#include "chrome/chrome_cleaner/logging/proto/shared_data.pb.h"
#include "chrome/chrome_cleaner/mojom/typemaps/footprints_mojom_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

using chrome_cleaner::mojom::FileInfoDataView;
using chrome_cleaner::mojom::FilePathDataView;
using chrome_cleaner::mojom::PUPDataView;
using chrome_cleaner::mojom::TraceLocationDataView;
using chrome_cleaner::FilePathSet;
using chrome_cleaner::PUPData;
using chrome_cleaner::UnorderedFilePathSet;

namespace {

bool ReadFilePathSetFromArrayDataView(
    ArrayDataView<FilePathDataView>* array_view,
    FilePathSet* out) {
  for (size_t i = 0; i < array_view->size(); ++i) {
    base::FilePath file_path;
    if (!array_view->Read(i, &file_path))
      return false;
    out->Insert(file_path);
  }
  return true;
}

}  // namespace

// static
int32_t StructTraits<chrome_cleaner::mojom::TraceLocationDataView,
                     chrome_cleaner::UwS::TraceLocation>::
    value(const chrome_cleaner::UwS::TraceLocation& location) {
  return static_cast<int32_t>(location);
}

// static
bool StructTraits<chrome_cleaner::mojom::TraceLocationDataView,
                  chrome_cleaner::UwS::TraceLocation>::
    Read(chrome_cleaner::mojom::TraceLocationDataView view,
         chrome_cleaner::UwS::TraceLocation* out) {
  if (!chrome_cleaner::UwS::TraceLocation_IsValid(view.value()))
    return false;
  *out = static_cast<chrome_cleaner::UwS::TraceLocation>(view.value());
  return true;
}

// static
const std::set<chrome_cleaner::UwS::TraceLocation>&
StructTraits<FileInfoDataView, PUPData::FileInfo>::found_in(
    const PUPData::FileInfo& info) {
  return info.found_in;
}

// static
bool StructTraits<FileInfoDataView, PUPData::FileInfo>::Read(
    FileInfoDataView view,
    PUPData::FileInfo* out) {
  ArrayDataView<TraceLocationDataView> found_in_view;
  view.GetFoundInDataView(&found_in_view);
  for (size_t i = 0; i < found_in_view.size(); ++i) {
    chrome_cleaner::UwS::TraceLocation location;
    if (!found_in_view.Read(i, &location))
      return false;
    out->found_in.insert(location);
  }
  return true;
}

// static
const UnorderedFilePathSet&
StructTraits<PUPDataView, PUPData::PUP>::expanded_disk_footprints(
    const PUPData::PUP& pup) {
  return pup.expanded_disk_footprints.file_paths();
}

// static
const PUPData::PUP::FileInfoMap::MapType&
StructTraits<PUPDataView, PUPData::PUP>::disk_footprints_info(
    const chrome_cleaner::PUPData::PUP& pup) {
  return pup.disk_footprints_info.map();
}

// static
bool StructTraits<PUPDataView, PUPData::PUP>::Read(PUPDataView view,
                                                   PUPData::PUP* out) {
  ArrayDataView<FilePathDataView> disk_view;
  view.GetExpandedDiskFootprintsDataView(&disk_view);
  if (!ReadFilePathSetFromArrayDataView(&disk_view,
                                        &out->expanded_disk_footprints)) {
    return false;
  }

  MapDataView<FilePathDataView, FileInfoDataView> disk_footprints_info_view;
  view.GetDiskFootprintsInfoDataView(&disk_footprints_info_view);
  for (size_t i = 0; i < disk_footprints_info_view.size(); ++i) {
    base::FilePath file_path;
    PUPData::FileInfo file_info;
    if (!disk_footprints_info_view.keys().Read(i, &file_path) ||
        !disk_footprints_info_view.values().Read(i, &file_info))
      return false;
    out->disk_footprints_info.Insert(file_path, file_info);
  }

  return true;
}

}  // namespace mojo
