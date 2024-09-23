// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_URL_CONSTANTS_H_
#define COMPONENTS_DOM_DISTILLER_CORE_URL_CONSTANTS_H_

namespace dom_distiller {

inline constexpr char kDomDistillerScheme[] = "chrome-distiller";
inline constexpr char kEntryIdKey[] = "entry_id";
inline constexpr char kUrlKey[] = "url";
inline constexpr char kTitleKey[] = "title";
inline constexpr char kTimeKey[] = "time";
inline constexpr char kViewerCssPath[] = "dom_distiller_viewer.css";
inline constexpr char kViewerLoadingImagePath[] =
    "dom_distiller_material_spinner.svg";
inline constexpr char kViewerSaveFontScalingPath[] = "savefontscaling/";

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_URL_CONSTANTS_H_
