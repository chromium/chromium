// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OMNIBOX_POPUP_FILE_SELECTOR_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OMNIBOX_POPUP_FILE_SELECTOR_H_

#include "base/memory/weak_ptr.h"
#include "components/lens/lens_bitmap_processing.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace content {
class WebContents;
}  // namespace content

namespace lens {
enum class MimeType;
}  // namespace lens

namespace base {
class UnguessableToken;
}

class OmniboxEditModel;

// Struct to store file data and mime type.
struct FileData {
  std::string bytes;
  std::string mime_type;
  std::string name;
};

class OmniboxPopupFileSelector : public ui::SelectFileDialog::Listener {
 public:
  // `owning_window` is the window that will be used to show the file selector
  // dialog.
  explicit OmniboxPopupFileSelector(gfx::NativeWindow owning_window);
  OmniboxPopupFileSelector(const OmniboxPopupFileSelector&) = delete;
  OmniboxPopupFileSelector& operator=(const OmniboxPopupFileSelector&) = delete;
  ~OmniboxPopupFileSelector() override;

  base::WeakPtr<OmniboxPopupFileSelector> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Virtual for testing.
  virtual void OpenFileUploadDialog(
      content::WebContents* web_contents,
      bool is_image,
      OmniboxEditModel* edit_model,
      std::optional<lens::ImageEncodingOptions> image_encoding_options,
      bool was_ai_mode_open);

  void OnFileDataReady(std::unique_ptr<FileData> file_data);

  void UpdateSearchboxContextData(lens::MimeType mime_type,
                                  std::string image_data_url,
                                  std::string file_name,
                                  std::string mime_string,
                                  const base::UnguessableToken& file_token);

  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

 private:
  scoped_refptr<ui::SelectFileDialog> file_dialog_;
  std::string file_info_type_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<OmniboxEditModel> edit_model_;
  std::optional<lens::ImageEncodingOptions> image_encoding_options_;
  gfx::NativeWindow owning_window_;
  bool was_ai_mode_open_ = false;

  base::WeakPtrFactory<OmniboxPopupFileSelector> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OMNIBOX_POPUP_FILE_SELECTOR_H_
