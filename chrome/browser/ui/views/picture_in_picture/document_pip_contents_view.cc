// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_contents_view.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/frame/scrim_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/metadata/metadata_impl_macros.h"

DocumentPipContentsView::DocumentPipContentsView(
    Profile* profile,
    std::unique_ptr<content::WebContents> child_web_contents)
    : views::WebView(profile) {
  dialog_scrim_ = AddChildView(std::make_unique<ScrimView>());
  SetOwnedWebContents(std::move(child_web_contents));
}

DocumentPipContentsView::~DocumentPipContentsView() = default;

void DocumentPipContentsView::SetDialogScrimVisible(bool visible) {
  dialog_scrim_->SetVisible(visible);
}

void DocumentPipContentsView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  views::WebView::OnBoundsChanged(previous_bounds);
  dialog_scrim_->SetBoundsRect(GetLocalBounds());
}

BEGIN_METADATA(DocumentPipContentsView)
END_METADATA
