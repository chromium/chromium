// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_contents_view.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"

DocumentPipContentsView::DocumentPipContentsView(
    Profile* profile,
    std::unique_ptr<content::WebContents> child_web_contents)
    : views::WebView(profile) {
  SetOwnedWebContents(std::move(child_web_contents));
}

DocumentPipContentsView::~DocumentPipContentsView() = default;

BEGIN_METADATA(DocumentPipContentsView)
END_METADATA
