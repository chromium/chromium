// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_BASE_FAVICON_TYPES_H_
#define COMPONENTS_FAVICON_BASE_FAVICON_TYPES_H_

#include <stdint.h>

#include <memory>

#include "base/containers/flat_set.h"
#include "base/memory/ref_counted_memory.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace favicon_base {

struct FallbackIconStyle;

using FaviconID = int64_t;

// Defines the icon types.
//
// IMPORTANT: these values must stay in sync with the FaviconType enum in
// tools/metrics/histograms/enums.xml.
// When you update the types please also check if it needs to be reflected in
// blink::mojom::FaviconIconType enums
//
// The values of the IconTypes are used to select the priority in which favicon
// data is returned.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.favicon
enum class IconType {
  kInvalid = 0,
  kFavicon,
  kTouchIcon,
  kTouchPrecomposedIcon,
  kWebManifestIcon,
  kMax = kWebManifestIcon,
  kCount
};

using IconTypeSet = base::flat_set<IconType>;

// Defines a gfx::Image of size desired_size_in_dip composed of image
// representations for each of the desired scale factors.
struct FaviconImageResult {
  FaviconImageResult();
  ~FaviconImageResult();

  // The resulting image.
  gfx::Image image;

  // The URL of the favicon which contains all of the image representations of
  // |image|.
  // TODO(pkotwicz): Return multiple |icon_urls| to allow |image| to have
  // representations from several favicons once content::FaviconStatus supports
  // multiple URLs.
  GURL icon_url;
};

// Defines a favicon bitmap which best matches the desired DIP size and one of
// the desired scale factors.
struct FaviconRawBitmapResult {
  FaviconRawBitmapResult();
  FaviconRawBitmapResult(const FaviconRawBitmapResult& other);
  ~FaviconRawBitmapResult();

  // Returns true if |bitmap_data| contains a valid bitmap.
  bool is_valid() const { return bitmap_data.get() && bitmap_data->size(); }

  // Indicates whether |bitmap_data| is expired.
  bool expired;

  // The bits of the bitmap.
  scoped_refptr<base::RefCountedMemory> bitmap_data;

  // The pixel dimensions of |bitmap_data|.
  gfx::Size pixel_size;

  // The URL of the containing favicon.
  GURL icon_url;

  // The icon type of the containing favicon.
  IconType icon_type;

  // Indicates whether the bitmap was fetched upon visiting a page. Value
  // false means that it was fetched on-demand by the UI of chrome, without
  // visiting the page.
  bool fetched_because_of_page_visit;
};

// Define type with same structure as FaviconRawBitmapResult for passing data to
// HistoryBackend::SetFavicons().
using FaviconRawBitmapData = FaviconRawBitmapResult;

// Result returned by LargeIconService::GetLargeIconOrFallbackStyle(). Contains
// either the bitmap data if the favicon database has a sufficiently large
// favicon bitmap and the style of the fallback icon otherwise.
struct LargeIconResult {
  explicit LargeIconResult(const FaviconRawBitmapResult& bitmap_in);

  // Takes ownership of |fallback_icon_style_in|.
  explicit LargeIconResult(FallbackIconStyle* fallback_icon_style_in);

  ~LargeIconResult();

  LargeIconResult(LargeIconResult&& result);

  // The bitmap from the favicon database if the database has a sufficiently
  // large one.
  FaviconRawBitmapResult bitmap;

  // The fallback icon style if a sufficiently large icon isn't available. This
  // uses the dominant color of a smaller icon as the background if available.
  std::unique_ptr<FallbackIconStyle> fallback_icon_style;
};

// Result returned by LargeIconService::GetLargeIconImageOrFallbackStyle().
// Contains either the gfx::Image if the favicon database has a sufficiently
// large favicon bitmap and the style of the fallback icon otherwise.
struct LargeIconImageResult {
  explicit LargeIconImageResult(const gfx::Image& image_in,
                                const GURL& icon_url_in);

  // Takes ownership of |fallback_icon_style_in|.
  explicit LargeIconImageResult(FallbackIconStyle* fallback_icon_style_in);

  ~LargeIconImageResult();

  // The image from the favicon database if the database has a sufficiently
  // large one.
  gfx::Image image;

  // The URL of the containing favicon. Specified only if |image| is not empty.
  GURL icon_url;

  // The fallback icon style if a sufficiently large icon isn't available. This
  // uses the dominant color of a smaller icon as the background if available.
  std::unique_ptr<FallbackIconStyle> fallback_icon_style;
};

// Enumeration listing all possible outcomes for fetch attempts from Google
// favicon server. Used for UMA enum GoogleFaviconServerRequestStatus, so do not
// change existing values. Insert new values at the end, and update the
// histogram definition.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.favicon
enum class GoogleFaviconServerRequestStatus {
  // Request sent out and the favicon successfully fetched.
  SUCCESS = 0,
  // Request sent out and a connection error occurred (no valid HTTP response
  // recevied).
  FAILURE_CONNECTION_ERROR = 1,
  // Request sent out and a HTTP error received.
  FAILURE_HTTP_ERROR = 2,
  // Request not sent out (previous HTTP error in cache).
  FAILURE_HTTP_ERROR_CACHED = 3,
  // Request sent out and favicon fetched but writing to database failed.
  FAILURE_ON_WRITE = 4,
  // Request not sent out (the request or the fetcher was invalid).
  DEPRECATED_FAILURE_INVALID = 5,
  // Request not sent out (the target URL was an IP address or its scheme was
  // not http(s)).
  FAILURE_TARGET_URL_SKIPPED = 6,
  // Request not sent out (the target URL was not valid).
  FAILURE_TARGET_URL_INVALID = 7,
  // Request not sent out (the server URL was not valid).
  FAILURE_SERVER_URL_INVALID = 8,
  // Request not sent out (as there already is an icon in the local favicon
  // database that prevents a new one to be stored).
  FAILURE_ICON_EXISTS_IN_DB = 9,
  // Insert new values here.
  COUNT
};

}  // namespace favicon_base

#endif  // COMPONENTS_FAVICON_BASE_FAVICON_TYPES_H_
