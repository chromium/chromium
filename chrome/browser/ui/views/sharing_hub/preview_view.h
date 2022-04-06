// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARING_HUB_PREVIEW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SHARING_HUB_PREVIEW_VIEW_H_

#include "ui/base/models/image_model.h"
#include "ui/views/view.h"
#include "url/gurl.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace sharing_hub {

// A PreviewView shows some information about a pending share so the user can
// tell what they're about to share. In particular, it looks like this:
//   +-----------------------------+
//   |+------+  Title              |
//   || Icon |                     |
//   |+------+  URL                |
//   +-----------------------------+
// The title and URL are fixed at construction time, but the icon may change -
// which image is used depends on the state of the DesktopSharePreview field
// trial.
class PreviewView : public views::View {
 public:
  // Taking an initial image here, instead of requiring the caller to call
  // OnImageChanged() after construction to set the initial image, means that
  // this class always has a valid image to display and does not have a
  // "half-initialized" state to worry about.
  explicit PreviewView(std::u16string title, GURL url, ui::ImageModel image);
  ~PreviewView() override;

  // This seemingly-odd method allows for PreviewView to be uncoupled from the
  // class that provides image updates - it receives image updates via a
  // callback which is bound by external code. Having PreviewView itself store
  // the subscription guarantees that the callback can't be delivered on a
  // deleted PreviewView.
  void TakeCallbackSubscription(base::CallbackListSubscription subscription);

  // Call this method to supply a new ImageModel to use for the preview image.
  // Whatever image you supply will be scaled to fit the image slot.
  void OnImageChanged(ui::ImageModel model);

 private:
  base::CallbackListSubscription subscription_;

  views::Label* title_ = nullptr;
  views::Label* url_ = nullptr;
  views::ImageView* image_ = nullptr;
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_UI_VIEWS_SHARING_HUB_PREVIEW_VIEW_H_
