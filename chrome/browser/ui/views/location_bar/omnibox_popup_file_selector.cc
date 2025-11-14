// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "net/base/mime_util.h"
#include "ui/base/base_window.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"

OmniboxPopupFileSelector::OmniboxPopupFileSelector() = default;

OmniboxPopupFileSelector::~OmniboxPopupFileSelector() = default;

void OmniboxPopupFileSelector::OpenFileUploadDialog(
    content::WebContents* web_contents,
    bool is_image) {
  file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));

  const ui::SelectFileDialog::Type dialog_type =
      ui::SelectFileDialog::SELECT_OPEN_FILE;

  ui::SelectFileDialog::FileTypeInfo file_types;
  if (is_image) {
    std::vector<base::FilePath::StringType> extensions;
    net::GetExtensionsForMimeType("image/*", &extensions);
    file_types.extensions.push_back(extensions);
    ;
  } else {
    file_types.extensions = {{FILE_PATH_LITERAL("pdf")}};
  }

  file_types.include_all_files = true;

  file_dialog_->SelectFile(dialog_type, /*title=*/u"", base::FilePath(),
                           &file_types, 0, base::FilePath::StringType(),
                           gfx::NativeWindow());
}

void OmniboxPopupFileSelector::FileSelected(const ui::SelectedFileInfo& file,
                                            int index) {}

void OmniboxPopupFileSelector::FileSelectionCanceled() {}
