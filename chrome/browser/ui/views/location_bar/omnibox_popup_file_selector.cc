// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/lens/contextual_input.h"
#include "components/lens/lens_bitmap_processing.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "content/public/browser/web_contents.h"
#include "net/base/mime_util.h"
#include "ui/base/base_window.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/selected_file_info.h"

OmniboxPopupFileSelector::OmniboxPopupFileSelector() = default;

OmniboxPopupFileSelector::~OmniboxPopupFileSelector() = default;

void OmniboxPopupFileSelector::OpenFileUploadDialog(
    content::WebContents* web_contents,
    bool is_image,
    contextual_search::ContextualSearchContextController* query_controller,
    OmniboxEditModel* edit_model,
    std::optional<lens::ImageEncodingOptions> image_encoding_options) {
  web_contents_ = web_contents;
  query_controller_ = query_controller;
  edit_model_ = edit_model;
  image_encoding_options_ = image_encoding_options;
  file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));

  const ui::SelectFileDialog::Type dialog_type =
      ui::SelectFileDialog::SELECT_OPEN_FILE;

  ui::SelectFileDialog::FileTypeInfo file_types;
  if (is_image) {
    std::vector<base::FilePath::StringType> extensions;
    net::GetExtensionsForMimeType("image/*", &extensions);
    file_types.extensions.push_back(extensions);
  } else {
    file_types.extensions = {{FILE_PATH_LITERAL("pdf")}};
  }

  file_types.include_all_files = true;

  file_dialog_->SelectFile(dialog_type, /*title=*/u"", base::FilePath(),
                           &file_types, 0, base::FilePath::StringType(),
                           gfx::NativeWindow());
}

std::unique_ptr<FileData> ReadFileAndProcess(const base::FilePath& local_path) {
  auto file_data = std::make_unique<FileData>();

  if (!base::ReadFileToString(local_path, &file_data->bytes)) {
    LOG(ERROR) << "Failed to read file from path: "
               << local_path.AsUTF8Unsafe();
  }
  net::GetMimeTypeFromExtension(local_path.Extension().substr(1),
                                &file_data->mime_type);
  file_data->name = local_path.BaseName().AsUTF8Unsafe();
  return file_data;
}

void OmniboxPopupFileSelector::FileSelected(const ui::SelectedFileInfo& file,
                                            int index) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ReadFileAndProcess, file.path()),
      base::BindOnce(&OmniboxPopupFileSelector::OnFileDataReady,
                     weak_factory_.GetWeakPtr()));
  file_dialog_.reset();
}

void OmniboxPopupFileSelector::FileSelectionCanceled() {}

void OmniboxPopupFileSelector::OnFileDataReady(
    std::unique_ptr<FileData> file_data) {
  if (!query_controller_) {
    return;
  }

  base::UnguessableToken file_token = base::UnguessableToken::Create();

  lens::MimeType mime_type;
  if (file_data->mime_type.find("pdf") != std::string::npos) {
    mime_type = lens::MimeType::kPdf;
  } else if (file_data->mime_type.find("image") != std::string::npos) {
    mime_type = lens::MimeType::kImage;
  } else {
    NOTREACHED();
  }

  std::unique_ptr<lens::ContextualInputData> input_data =
      std::make_unique<lens::ContextualInputData>();
  input_data->context_input = std::vector<lens::ContextualInput>();
  input_data->primary_content_type = mime_type;

  base::span<const uint8_t> file_data_span =
      base::as_bytes(base::span(file_data->bytes));
  std::vector<uint8_t> file_data_vector(file_data_span.begin(),
                                        file_data_span.end());
  input_data->context_input->push_back(
      lens::ContextualInput(std::move(file_data_vector), mime_type));

  query_controller_->StartFileUploadFlow(file_token, std::move(input_data),
                                         std::move(image_encoding_options_));

  std::string image_data_url;
  if (mime_type == lens::MimeType::kImage) {
    image_data_url = "data:" + file_data->mime_type + ";base64," +
                     base::Base64Encode(file_data->bytes);
  }

  UpdateSearchboxContextData(file_token, mime_type, image_data_url,
                             file_data->name, file_data->mime_type);

  edit_model_->OpenAiMode(false, /*via_context_menu=*/true);
}

void OmniboxPopupFileSelector::UpdateSearchboxContextData(
    base::UnguessableToken file_token,
    lens::MimeType mime_type,
    const std::string& image_data_url,
    std::string file_name,
    std::string mime_string) {
  auto file_attachment = searchbox::mojom::FileAttachmentStub::New();
  file_attachment->uuid = file_token;
  file_attachment->name = file_name;
  file_attachment->mime_type = mime_string;

  if (mime_type == lens::MimeType::kImage) {
    file_attachment->image_data_url = image_data_url;
  }

  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_);
  if (!browser_window_interface) {
    return;
  }
  SearchboxContextData* searchbox_context_data =
      browser_window_interface->GetFeatures().searchbox_context_data();
  if (!searchbox_context_data) {
    return;
  }
  auto context = searchbox_context_data->TakePendingContext();
  if (!context) {
    context = std::make_unique<SearchboxContextData::Context>();
  }
  context->file_infos.push_back(
      searchbox::mojom::SearchContextAttachmentStub::NewFileAttachment(
          std::move(file_attachment)));
  searchbox_context_data->SetPendingContext(std::move(context));
}
