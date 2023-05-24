// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEARBY_SHARE_NEARBY_SHARE_MOJOM_TRAITS_H_
#define CHROME_BROWSER_UI_WEBUI_NEARBY_SHARE_NEARBY_SHARE_MOJOM_TRAITS_H_

#include <string>

#include "chrome/browser/nearby_sharing/share_target.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share.mojom.h"
#include "mojo/public/cpp/bindings/optional_as_pointer.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "url/gurl.h"

namespace mojo {

template <>
struct StructTraits<nearby_share::mojom::ShareTargetDataView, ShareTarget> {
  static const base::UnguessableToken& id(const ShareTarget& share_target);
  static const std::string& name(const ShareTarget& share_target);
  static nearby_share::mojom::ShareTargetType type(
      const ShareTarget& share_target);
  static mojo::OptionalAsPointer<const GURL> image_url(
      const ShareTarget& share_target);
  static nearby_share::mojom::PayloadPreviewPtr payload_preview(
      const ShareTarget& share_target);
  static bool for_self_share(const ShareTarget& share_target);
  static bool Read(nearby_share::mojom::ShareTargetDataView data,
                   ShareTarget* out);
};

}  // namespace mojo

#endif  // CHROME_BROWSER_UI_WEBUI_NEARBY_SHARE_NEARBY_SHARE_MOJOM_TRAITS_H_
