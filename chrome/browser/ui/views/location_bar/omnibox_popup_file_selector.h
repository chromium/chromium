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

namespace contextual_search {
class ContextualSearchContextController;
}  // namespace contextual_search

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
  OmniboxPopupFileSelector();
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
      contextual_search::ContextualSearchContextController* query_controller,
      OmniboxEditModel* edit_model,
      std::optional<lens::ImageEncodingOptions> image_encoding_options);

  void OnFileDataReady(std::unique_ptr<FileData> file_data);

  void UpdateSearchboxContextData(base::UnguessableToken file_token,
                                  lens::MimeType mime_type,
                                  const std::string& image_data_url,
                                  std::string file_name,
                                  std::string mime_string);

  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

 private:
  scoped_refptr<ui::SelectFileDialog> file_dialog_;
  raw_ptr<contextual_search::ContextualSearchContextController>
      query_controller_;
  std::string file_info_type_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<OmniboxEditModel> edit_model_;
  std::optional<lens::ImageEncodingOptions> image_encoding_options_;

  base::WeakPtrFactory<OmniboxPopupFileSelector> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OMNIBOX_POPUP_FILE_SELECTOR_H_
