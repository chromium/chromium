// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_contents_wrapper.h"

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"

ReadAnythingContentsWrapper::ReadAnythingContentsWrapper() = default;
ReadAnythingContentsWrapper::ReadAnythingContentsWrapper(Ptr wrapper)
    : wrapper_(std::move(wrapper)) {}
ReadAnythingContentsWrapper::ReadAnythingContentsWrapper(
    ReadAnythingContentsWrapper&&) noexcept = default;
ReadAnythingContentsWrapper& ReadAnythingContentsWrapper::operator=(
    ReadAnythingContentsWrapper&&) noexcept = default;
ReadAnythingContentsWrapper::~ReadAnythingContentsWrapper() = default;
