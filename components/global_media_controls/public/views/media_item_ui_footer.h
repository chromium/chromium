// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_FOOTER_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_FOOTER_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace global_media_controls {

// A MediaItemUIFooter is a type of views::View that can be inserted at the
// bottom of a MediaItemUI. Users of global media controls can create views that
// extend this class that will be inserted into the MediaItemUI and receive
// color updates.
class COMPONENT_EXPORT(GLOBAL_MEDIA_CONTROLS) MediaItemUIFooter
    : public views::View {
  METADATA_HEADER(MediaItemUIFooter, views::View)

 public:
  virtual void OnColorsChanged(SkColor foreground, SkColor background) = 0;
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_FOOTER_H_
