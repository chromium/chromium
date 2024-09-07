// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_PROTO_CONVERTER_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_PROTO_CONVERTER_H_

#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"

namespace lens {

// Returns a list of overlay object mojom pointers created from the lens overlay
// server response.
std::vector<lens::mojom::OverlayObjectPtr>
CreateObjectsMojomArrayFromServerResponse(
    const lens::LensOverlayServerResponse& response);

// Returns a text mojom object pointer from a lens overlay server response.
// Returns a null text ptr if there is no text in response.
lens::mojom::TextPtr CreateTextMojomFromServerResponse(
    const lens::LensOverlayServerResponse& response,
    const gfx::Size resized_bitmap_size);
}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_PROTO_CONVERTER_H_
