// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OMNIBOX_POPUP_FILE_SELECTOR_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OMNIBOX_POPUP_FILE_SELECTOR_H_

#include "base/memory/weak_ptr.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace content {
class WebContents;
}  // namespace content

class OmniboxPopupFileSelector : public ui::SelectFileDialog::Listener {
 public:
  OmniboxPopupFileSelector();
  OmniboxPopupFileSelector(const OmniboxPopupFileSelector&) = delete;
  OmniboxPopupFileSelector& operator=(const OmniboxPopupFileSelector&) = delete;
  ~OmniboxPopupFileSelector() override;

  void OpenFileUploadDialog(content::WebContents* web_contents, bool is_image);

  base::WeakPtr<OmniboxPopupFileSelector> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

 private:
  scoped_refptr<ui::SelectFileDialog> file_dialog_;

  base::WeakPtrFactory<OmniboxPopupFileSelector> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OMNIBOX_POPUP_FILE_SELECTOR_H_
