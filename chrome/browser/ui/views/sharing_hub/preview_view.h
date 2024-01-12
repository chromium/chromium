// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARING_HUB_PREVIEW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_SHARING_HUB_PREVIEW_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/share/share_attempt.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

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
// The title, URL, and icon are all fixed at construction time.
class PreviewView : public views::View {
  METADATA_HEADER(PreviewView, views::View)

 public:
  explicit PreviewView(share::ShareAttempt attempt);
  ~PreviewView() override;

 private:
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> url_ = nullptr;
  raw_ptr<views::ImageView> image_ = nullptr;
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_UI_VIEWS_SHARING_HUB_PREVIEW_VIEW_H_
