// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_DISTILLER_CORE_URL_CONSTANTS_H_
#define COMPONENTS_WEB_DISTILLER_CORE_URL_CONSTANTS_H_

namespace web_distiller {

inline constexpr char kWebDistillerScheme[] = "chrome-distiller";
inline constexpr char kEntryIdKey[] = "entry_id";
inline constexpr char kUrlKey[] = "url";
inline constexpr char kTitleKey[] = "title";
inline constexpr char kTimeKey[] = "time";
inline constexpr char kViewerCssPath[] = "dom_distiller_viewer.css";
inline constexpr char kViewerLoadingImagePath[] =
    "dom_distiller_material_spinner.svg";
inline constexpr char kViewerSaveFontScalingPath[] = "savefontscaling/";

}  // namespace web_distiller

#endif  // COMPONENTS_WEB_DISTILLER_CORE_URL_CONSTANTS_H_
