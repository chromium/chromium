// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_FOOTER_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_FOOTER_H_

#include "ui/views/view.h"

namespace global_media_controls {

// A MediaItemUIFooter is a type of views::View that can be inserted at the
// bottom of a MediaItemUI. Users of global media controls can create views that
// extend this class that will be inserted into the MediaItemUI and receive
// color updates.
class MediaItemUIFooter : public views::View {
 public:
  virtual void OnColorsChanged(SkColor foreground, SkColor background) = 0;
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_VIEWS_MEDIA_ITEM_UI_FOOTER_H_
