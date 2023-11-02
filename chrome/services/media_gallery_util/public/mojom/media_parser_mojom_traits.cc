// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/public/mojom/media_parser_mojom_traits.h"

#include "mojo/public/cpp/bindings/array_data_view.h"

namespace {

using TypeImageDataView = chrome::mojom::AttachedImageDataView;
using TypeImage = ::metadata::AttachedImage;

}  // namespace

namespace mojo {

// static
bool StructTraits<TypeImageDataView, TypeImage>::Read(TypeImageDataView view,
                                                      TypeImage* out) {
  if (!view.ReadType(&out->type))
    return false;

  ArrayDataView<uint8_t> data;
  view.GetDataDataView(&data);

  out->data.assign(reinterpret_cast<const char*>(data.data()), data.size());
  return true;
}

}  // namespace mojo
