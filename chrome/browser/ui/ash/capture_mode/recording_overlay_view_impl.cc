// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/capture_mode/recording_overlay_view_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/projector/projector_client_impl.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"

RecordingOverlayViewImpl::RecordingOverlayViewImpl(Profile* profile)
    : web_view_(AddChildView(std::make_unique<views::WebView>(profile))) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Use a transparent background for the web contents.
  auto* web_contents = web_view_->GetWebContents();
  DCHECK(web_contents);
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      web_contents, SK_ColorTRANSPARENT);

  // Loading the annotator app in `web_view_` can take a long time, so in order
  // to avoid stalling the initialization of recording, we will do this
  // asynchronously.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&RecordingOverlayViewImpl::InitializeAnnotator,
                                weak_ptr_factory_.GetWeakPtr()));
}

RecordingOverlayViewImpl::~RecordingOverlayViewImpl() = default;

void RecordingOverlayViewImpl::InitializeAnnotator() {
  ProjectorClientImpl::InitForProjectorAnnotator(web_view_);
}

BEGIN_METADATA(RecordingOverlayViewImpl, ash::RecordingOverlayView)
END_METADATA
