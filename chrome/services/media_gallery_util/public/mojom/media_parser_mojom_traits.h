// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_MEDIA_GALLERY_UTIL_PUBLIC_MOJOM_MEDIA_PARSER_MOJOM_TRAITS_H_
#define CHROME_SERVICES_MEDIA_GALLERY_UTIL_PUBLIC_MOJOM_MEDIA_PARSER_MOJOM_TRAITS_H_

#include <string>

#include "base/containers/span.h"
#include "chrome/common/media_galleries/metadata_types.h"
#include "chrome/services/media_gallery_util/public/mojom/media_parser.mojom-shared.h"
#include "mojo/public/cpp/bindings/array_traits_span.h"

namespace mojo {

template <>
struct StructTraits<chrome::mojom::AttachedImageDataView,
                    ::metadata::AttachedImage> {
  static const std::string& type(const ::metadata::AttachedImage& image) {
    return image.type;
  }

  static base::span<const uint8_t> data(
      const ::metadata::AttachedImage& image) {
    // TODO(dcheng): perhaps metadata::AttachedImage should consider passing the
    // image data around in a std::vector<uint8_t>.
    return base::as_byte_span(image.data);
  }

  static bool Read(chrome::mojom::AttachedImageDataView view,
                   ::metadata::AttachedImage* out);
};

}  // namespace mojo

#endif  // CHROME_SERVICES_MEDIA_GALLERY_UTIL_PUBLIC_MOJOM_MEDIA_PARSER_MOJOM_TRAITS_H_
