// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/main_container_view.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "ui/base/metadata/metadata_impl_macros.h"

MainContainerView::MainContainerView(BrowserView& browser_view)
    : browser_view_(browser_view) {}
MainContainerView::~MainContainerView() = default;

BEGIN_METADATA(MainContainerView)
END_METADATA
