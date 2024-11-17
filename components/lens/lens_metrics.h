// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LENS_LENS_METRICS_H_
#define COMPONENTS_LENS_LENS_METRICS_H_

namespace lens {

// Histogram for recording ambient search queries.
constexpr char kAmbientSearchQueryHistogramName[] = "Search.Ambient.Query";

// Histogram for recording camera open events.
constexpr char kSearchCameraOpenHistogramName[] = "Search.Image.Camera.Open";

// Histogram for recording camera result events.
constexpr char kSearchCameraResultHistogramName[] =
    "Search.Image.Camera.Result";

// Histogram for recording the capture result of Lens Region Search. See enum
// below for types of results.
constexpr char kLensRegionSearchCaptureResultHistogramName[] =
    "Search.RegionSearch.Lens.Result";

// Histogram for recording the viewport proportion in relation to region
// selected for the Lens Region Search feature.
constexpr char kLensRegionSearchRegionViewportProportionHistogramName[] =
    "Search.RegionSearch.Lens.RegionViewportProportion";

// Histogram for recording the aspect ratio of the captured region.
constexpr char kLensRegionSearchRegionAspectRatioHistogramName[] =
    "Search.RegionSearch.Lens.RegionAspectRatio";

// Needs to be kept in sync with CameraOpenEntryPoint enum in
// tools/metrics/histograms/enums.xml.
enum class CameraOpenEntryPoint {
  OMNIBOX = 0,
  NEW_TAB_PAGE = 1,
  WIDGET = 2,
  TASKS_SURFACE = 3,
  KEYBOARD = 4,
  SPOTLIGHT = 5,
  APP_ICON_LONG_PRESS = 6,
  PLUS_BUTTON = 7,
  WEB_SEARCH_BAR = 8,
  TRANSLATE_ONEBOX = 9,
  INTENTS = 10,
  WEB_IMAGES_SEARCH_BAR = 11,
  WHATS_NEW_PROMO = 12,
  kMaxValue = WHATS_NEW_PROMO
};

// Needs to be kept in sync with CameraResult enum in
// tools/metrics/histograms/enums.xml.
enum class CameraResult {
  SUCCESS_CAMERA = 0,
  SUCCESS_GALLERY_IMAGE = 1,
  SUCCESS_QR_CODE = 2,
  DISMISSED = 3,
  kMaxValue = DISMISSED
};

// Needs to be kept in sync with AmbientSearchEntryPoint enum in
// tools/metrics/histograms/enums.xml.
enum class AmbientSearchEntryPoint {
  CONTEXT_MENU_SEARCH_IMAGE_WITH_GOOGLE_LENS = 0,
  CONTEXT_MENU_SEARCH_IMAGE_WITH_WEB = 1,
  CONTEXT_MENU_SEARCH_REGION_WITH_GOOGLE_LENS = 2,
  CONTEXT_MENU_SEARCH_REGION_WITH_WEB = 3,
  CONTEXT_MENU_SEARCH_WEB_FOR = 4,
  OMNIBOX = 5,
  NEW_TAB_PAGE = 6,
  QUICK_ACTION_SEARCH_WIDGET = 7,
  KEYBOARD = 8,
  APP_ICON_LONG_PRESS = 9,
  SPOTLIGHT = 10,
  PLUS_BUTTON = 11,
  WEB_SEARCH_BAR = 12,
  COMPANION_REGION_SEARCH = 13,
  TRANSLATE_ONEBOX = 14,
  INTENTS = 15,
  WEB_IMAGES_SEARCH_BAR = 16,
  WHATS_NEW_PROMO = 17,
  CONTEXT_MENU_SEARCH_VIDEO_FRAME_WITH_GOOGLE_LENS = 18,
  CONTEXT_MENU_SEARCH_VIDEO_FRAME_WITH_WEB = 19,
  LENS_OVERLAY_LOCATION_BAR = 20,
  LENS_OVERLAY_OVERFLOW_MENU = 21,
  CONTEXT_MENU_SEARCH_IMAGE_WITH_LENS_OVERLAY = 22,
  CONTEXT_MENU_SEARCH_REGION_WITH_LENS_OVERLAY = 23,
  CONTEXT_MENU_SEARCH_VIDEO_FRAME_WITH_LENS_OVERLAY = 24,
  CONTEXT_MENU_SEARCH_IMAGE_WITH_LENS_OVERLAY_ACCESSIBILITY_FALLBACK = 25,
  CONTEXT_MENU_SEARCH_REGION_WITH_LENS_OVERLAY_ACCESSIBILITY_FALLBACK = 26,
  CONTEXT_MENU_SEARCH_VIDEO_WITH_LENS_OVERLAY_ACCESSIBILITY_FALLBACK = 27,
  LENS_OVERLAY_LOCATION_BAR_ACCESSIBILITY_FALLBACK = 28,
  kMaxValue = LENS_OVERLAY_LOCATION_BAR_ACCESSIBILITY_FALLBACK
};

// This should be kept in sync with the LensRegionSearchAspectRatio enum
// in tools/metrics/histograms/enums.xml. The aspect ratios are defined as:
//  SQUARE: [0.8, 1.2]
//  WIDE: (1.2, 1.7]
//  VERY_WIDE: (1.7, infinity)
//  TALL: [0.3, 0.8)
//  VERY_TALL: [0, 0.3)
enum class LensRegionSearchAspectRatio {
  UNDEFINED = 0,
  SQUARE = 1,
  WIDE = 2,
  VERY_WIDE = 3,
  TALL = 4,
  VERY_TALL = 5,
  kMaxValue = VERY_TALL
};

// This should be kept in sync with the LensRegionSearchCaptureResult enum
// in tools/metrics/histograms/enums.xml.
enum class LensRegionSearchCaptureResult {
  SUCCESS = 0,
  FAILED_TO_OPEN_TAB = 1,
  ERROR_CAPTURING_REGION = 2,
  USER_EXITED_CAPTURE_ESCAPE = 3,
  USER_EXITED_CAPTURE_CLOSE_BUTTON = 4,
  USER_NAVIGATED_FROM_CAPTURE = 5,
  kMaxValue = USER_NAVIGATED_FROM_CAPTURE
};

// Record an ambient search query along with the entry point that initiated.
extern void RecordAmbientSearchQuery(AmbientSearchEntryPoint entry_point);

// Record a camera open event with the entry point.
extern void RecordCameraOpen(CameraOpenEntryPoint entry_point);

// Record a camera result.
extern void RecordCameraResult(CameraResult result);

}  // namespace lens

#endif  // COMPONENTS_LENS_LENS_METRICS_H_
