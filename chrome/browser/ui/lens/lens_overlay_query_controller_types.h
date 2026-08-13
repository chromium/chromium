// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LENS_LENS_OVERLAY_QUERY_CONTROLLER_TYPES_H_
#define CHROME_BROWSER_UI_LENS_LENS_OVERLAY_QUERY_CONTROLLER_TYPES_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom-forward.h"
#include "chrome/browser/lens/core/mojom/text.mojom-forward.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace lens {

// Data struct representing content data to be sent to the Lens server.
struct PageContent {
  PageContent();
  PageContent(std::vector<uint8_t> bytes, lens::MimeType content_type);
  PageContent(const PageContent& other);
  ~PageContent();

 public:
  std::vector<uint8_t> bytes_;
  lens::MimeType content_type_;
};

// Callback type alias for the lens overlay full image response.
using LensOverlayFullImageResponseCallback =
    base::RepeatingCallback<void(std::vector<lens::mojom::OverlayObjectPtr>,
                                 lens::mojom::TextPtr,
                                 bool)>;

// Callback type alias for the lens overlay url response.
using LensOverlayUrlResponseCallback =
    base::RepeatingCallback<void(lens::proto::LensOverlayUrlResponse)>;

// Callback type alias for the lens overlay interaction response.
using LensOverlayInteractionResponseCallback =
    base::RepeatingCallback<void(lens::mojom::TextPtr)>;

// Callback type alias for the thumbnail image creation.
using LensOverlayThumbnailCreatedCallback =
    base::RepeatingCallback<void(const std::string&, const SkBitmap&)>;

// Callback type alias for the OAuth headers created.
using OAuthHeadersCreatedCallback =
    base::OnceCallback<void(std::vector<std::string>)>;

using UploadProgressCallback =
    base::RepeatingCallback<void(uint64_t position, uint64_t total)>;

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_OVERLAY_QUERY_CONTROLLER_TYPES_H_
