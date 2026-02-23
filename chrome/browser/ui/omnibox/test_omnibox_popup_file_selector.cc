// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/test_omnibox_popup_file_selector.h"

#include <optional>

#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"
#include "components/lens/lens_bitmap_processing.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/native_ui_types.h"

TestOmniboxPopupFileSelector::TestOmniboxPopupFileSelector(
    gfx::NativeWindow owning_window)
    : OmniboxPopupFileSelector(owning_window) {}

TestOmniboxPopupFileSelector::~TestOmniboxPopupFileSelector() = default;

void TestOmniboxPopupFileSelector::OpenFileUploadDialog(
    content::WebContents* web_contents,
    bool is_image,
    OmniboxEditModel* edit_model,
    std::optional<lens::ImageEncodingOptions> image_encoding_options,
    bool was_ai_mode_open) {
  open_file_upload_dialog_calls_++;
  last_was_ai_mode_open_ = was_ai_mode_open;
  edit_model_ = edit_model;
}

void TestOmniboxPopupFileSelector::FileSelectionCanceled() {
  if (last_was_ai_mode_open_ && edit_model_) {
    edit_model_->OpenAiMode(false, true);
  }
}
