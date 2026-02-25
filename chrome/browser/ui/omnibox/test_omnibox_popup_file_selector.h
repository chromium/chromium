// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_TEST_OMNIBOX_POPUP_FILE_SELECTOR_H_
#define CHROME_BROWSER_UI_OMNIBOX_TEST_OMNIBOX_POPUP_FILE_SELECTOR_H_

#include <optional>

#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"
#include "components/lens/lens_bitmap_processing.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/native_ui_types.h"

class TestOmniboxPopupFileSelector : public OmniboxPopupFileSelector {
 public:
  explicit TestOmniboxPopupFileSelector(gfx::NativeWindow owning_window);

  ~TestOmniboxPopupFileSelector() override;

  void OpenFileUploadDialog(
      content::WebContents* web_contents,
      bool is_image,
      OmniboxEditModel* edit_model,
      std::optional<lens::ImageEncodingOptions> image_encoding_options,
      bool was_ai_mode_open) override;

  void FileSelectionCanceled() override;

  int open_file_upload_dialog_calls() const {
    return open_file_upload_dialog_calls_;
  }

  bool last_was_ai_mode_open() const { return last_was_ai_mode_open_; }

 private:
  int open_file_upload_dialog_calls_ = 0;
  bool last_was_ai_mode_open_ = false;
  raw_ptr<OmniboxEditModel> edit_model_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_OMNIBOX_TEST_OMNIBOX_POPUP_FILE_SELECTOR_H_
