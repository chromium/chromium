// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OMNIBOX_POPUP_FILE_SELECTOR_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OMNIBOX_POPUP_FILE_SELECTOR_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/contextual_search/contextual_search_types.h"
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

class OmniboxPopupDeactivationBlocker;

class OmniboxPopupFileSelector : public ui::SelectFileDialog::Listener {
 public:
  // `owning_window` is the window that will be used to show the file selector
  // dialog.
  explicit OmniboxPopupFileSelector(gfx::NativeWindow owning_window);
  OmniboxPopupFileSelector(const OmniboxPopupFileSelector&) = delete;
  OmniboxPopupFileSelector& operator=(const OmniboxPopupFileSelector&) = delete;
  ~OmniboxPopupFileSelector() override;

  // Helper to create image encoding options from the Omnibox feature config.
  static std::optional<lens::ImageEncodingOptions> CreateImageEncodingOptions();

  base::WeakPtr<OmniboxPopupFileSelector> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void set_open_ai_mode_callback(base::RepeatingClosure callback) {
    open_ai_mode_callback_ = std::move(callback);
  }
  void set_file_chooser_opened_callback(base::RepeatingClosure callback) {
    file_chooser_opened_callback_ = std::move(callback);
  }
  void set_file_chooser_closed_callback(base::RepeatingClosure callback) {
    file_chooser_closed_callback_ = std::move(callback);
  }

  // Virtual for testing.
  virtual void OpenFileUploadDialog(
      content::WebContents* web_contents,
      bool is_image,
      OmniboxEditModel* edit_model,
      std::optional<lens::ImageEncodingOptions> image_encoding_options,
      bool was_ai_mode_open);

  void OnFileDataReady(std::unique_ptr<FileData> file_data);

  void UpdateSearchboxContextData(
      lens::MimeType mime_type,
      const std::string& image_data_url,
      const std::string& file_name,
      const std::string& mime_string,
      base::expected<base::UnguessableToken,
                     contextual_search::ContextUploadErrorType> result);

  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void MultiFilesSelected(
      const std::vector<ui::SelectedFileInfo>& files) override;
  void FileSelectionCanceled() override;

 private:
  scoped_refptr<ui::SelectFileDialog> file_dialog_;
  std::string file_info_type_;
  base::WeakPtr<content::WebContents> web_contents_;
  raw_ptr<OmniboxEditModel> edit_model_;
  std::optional<lens::ImageEncodingOptions> image_encoding_options_;
  gfx::NativeWindow owning_window_;
  bool was_ai_mode_open_ = false;
  bool is_image_ = false;

  base::RepeatingClosure open_ai_mode_callback_;
  base::RepeatingClosure file_chooser_opened_callback_;
  base::RepeatingClosure file_chooser_closed_callback_;

  // Prevents the omnibox popup from closing when focus shifts to the system
  // file dialog.
  std::unique_ptr<OmniboxPopupDeactivationBlocker> deactivation_blocker_;

  void NotifyFileSelectionClosed();
  void OpenAiMode();

  base::WeakPtrFactory<OmniboxPopupFileSelector> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_OMNIBOX_POPUP_FILE_SELECTOR_H_
