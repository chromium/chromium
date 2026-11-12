// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"

#include <algorithm>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/select_file_policy/chrome_select_file_policy.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_host.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_aim_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/lens/lens_bitmap_processing.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/omnibox/common/input_state.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/omnibox/common/omnibox_metrics_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "net/base/mime_util.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace {

// Returns the location bar of the browser window that `web_contents` belongs
// to, or null if there is none (such as for Omnibox Everywhere).
LocationBar* GetLocationBar(content::WebContents* web_contents) {
  if (!web_contents ||
      OmniboxContextMenuController::GetOmniboxEverywhereUI(web_contents)) {
    return nullptr;
  }
  auto* browser_window = webui::GetBrowserWindowInterface(web_contents);
  return browser_window
             ? BrowserWindow::FromBrowser(browser_window)->GetLocationBar()
             : nullptr;
}

}  // namespace

OmniboxPopupFileSelector::OmniboxPopupFileSelector(
    gfx::NativeWindow owning_window)
    : owning_window_(owning_window) {}

OmniboxPopupFileSelector::~OmniboxPopupFileSelector() {
  if (file_dialog_) {
    file_dialog_->ListenerDestroyed();
  }
}

std::optional<lens::ImageEncodingOptions>
OmniboxPopupFileSelector::CreateImageEncodingOptions() {
  // TODO(crbug.com/457815342): Use omnibox fieldtrial when available.
  auto image_upload_config =
      omnibox::FeatureConfig::Get().config.composebox().image_upload();
  return lens::ImageEncodingOptions{
      .max_size = image_upload_config.downscale_max_image_size(),
      .max_height = image_upload_config.downscale_max_image_height(),
      .max_width = image_upload_config.downscale_max_image_width(),
      .compression_quality = image_upload_config.image_compression_quality()};
}

void OmniboxPopupFileSelector::OpenFileUploadDialog(
    content::WebContents* web_contents,
    bool is_image,
    OmniboxEditModel* edit_model,
    std::optional<lens::ImageEncodingOptions> image_encoding_options,
    bool was_ai_mode_open) {
  // Close any dialog that is still open first, so that the popup that opened it
  // is told it closed.
  CancelFileSelection();
  web_contents_ = web_contents ? web_contents->GetWeakPtr() : nullptr;
  edit_model_ = edit_model;
  image_encoding_options_ = image_encoding_options;
  was_ai_mode_open_ = was_ai_mode_open;
  is_image_ = is_image;
  if (file_chooser_opened_callback_) {
    file_chooser_opened_callback_.Run();
  }
  if (LocationBar* location_bar = GetLocationBar(web_contents)) {
    if (was_ai_mode_open) {
      if (auto* delegate = location_bar->GetPresenterDelegate()) {
        if (auto* presenter = delegate->GetOmniboxPopupAimPresenter()) {
          deactivation_blocker_ = presenter->CreateDeactivationBlocker();
        }
      }
    } else if (auto* popup_view = location_bar->GetOmniboxPopupView()) {
      if (OmniboxPopupHost* popup_host = popup_view->popup_host()) {
        popup_host->OnFileDialogOpened();
        deactivation_blocker_ = popup_host->CreateDeactivationBlocker();
      }
    }
  }

  file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));

  const ui::SelectFileDialog::Type dialog_type =
      ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE;

  ui::SelectFileDialog::FileTypeInfo file_types;
  if (is_image) {
    std::vector<base::FilePath::StringType> extensions;
    net::GetExtensionsForMimeType("image/*", &extensions);
    // HEIC/HEIF extensions are only known to `net` via the platform MIME
    // database, which may not include them (e.g. Windows without the HEIF
    // extension installed). Add them explicitly so they are not filtered out.
    for (const auto* heif_extension :
         {FILE_PATH_LITERAL("heic"), FILE_PATH_LITERAL("heif")}) {
      if (!std::ranges::contains(extensions, heif_extension)) {
        extensions.emplace_back(heif_extension);
      }
    }
    file_types.extensions.push_back(extensions);
  } else if (!lens::features::IsLensSendRawFileMediaTypesEnabled()) {
    file_types.extensions = {{FILE_PATH_LITERAL("pdf")}};
  }

  file_types.include_all_files = true;

  file_dialog_->SelectFile(dialog_type, /*title=*/u"", base::FilePath(),
                           &file_types, 0, base::FilePath::StringType(),
                           owning_window_);
}

void OmniboxPopupFileSelector::CancelFileSelection() {
  if (!file_dialog_) {
    return;
  }
  file_dialog_->ListenerDestroyed();
  file_dialog_.reset();
  deactivation_blocker_.reset();
  NotifyFileSelectionClosed(/*file_selected=*/false);
}

namespace {

using FileData = OmniboxPopupFileSelector::FileData;

constexpr size_t kDefaultMaxNumFiles = omnibox::kDefaultMaxTotalInputs;
constexpr size_t kDefaultMaxFileSizeBytes = 15 * 1024 * 1024;

size_t GetMaxAttachmentSizeBytes() {
  size_t max_size = ntp_composebox::FeatureConfig::Get()
                        .config.composebox()
                        .attachment_upload()
                        .max_size_bytes();
  return max_size > 0 ? max_size : kDefaultMaxFileSizeBytes;
}

// Returns the MIME type for `path`. Set `allow_platform_lookup` to false when
// calling from the UI thread, since the platform lookup can block on disk I/O
// (e.g. XDG mime.cache on Linux).
std::string GetMimeTypeForFile(const base::FilePath& path,
                               bool is_image,
                               bool allow_platform_lookup = true) {
  std::string mime_type;
  base::FilePath::StringType ext = path.FinalExtension();
  if (ext.starts_with(FILE_PATH_LITERAL('.'))) {
    if (allow_platform_lookup) {
      net::GetMimeTypeFromExtension(ext.substr(1), &mime_type);
    } else {
      net::GetWellKnownMimeTypeFromExtension(ext.substr(1), &mime_type);
    }
  }
  if (!mime_type.empty()) {
    return mime_type;
  }
  // Some platforms do not report a MIME type for HEIC/HEIF files (e.g.
  // uppercase `.HEIC` on Linux, or Windows without the HEIF extension
  // installed). Infer it so these are not mislabeled as PNG and decoded.
  if (path.MatchesExtension(FILE_PATH_LITERAL(".heic"))) {
    return "image/heic";
  }
  if (path.MatchesExtension(FILE_PATH_LITERAL(".heif"))) {
    return "image/heif";
  }
  return is_image ? "image/png" : "application/pdf";
}

std::unique_ptr<FileData> ReadFileData(const base::FilePath& path,
                                       size_t max_attachment_size,
                                       bool is_image) {
  auto file_data = std::make_unique<FileData>();
  file_data->path = path;
  file_data->mime_type = GetMimeTypeForFile(path, is_image);
  // Decide by MIME type, not `is_image`. The image picker allows all files,
  // and `AddFileContextFromBrowser()` also uses the MIME type.
  const uint64_t max_file_size = omnibox::GetComposeboxMaxFileSizeBytes(
      file_data->mime_type, /*is_image=*/false, max_attachment_size);

  std::optional<int64_t> file_size = base::GetFileSize(path);
  if (file_size.has_value()) {
    if (file_size.value() == 0) {
      file_data->error = contextual_search::ContextUploadErrorType::
          kBrowserProcessingFileEmptyError;
      return file_data;
    }
    if (static_cast<uint64_t>(file_size.value()) > max_file_size) {
      file_data->error = contextual_search::ContextUploadErrorType::
          kBrowserProcessingFileTooLargeError;
      return file_data;
    }
  }

  if (!base::ReadFileToString(path, &file_data->bytes)) {
    file_data->error =
        contextual_search::ContextUploadErrorType::kBrowserProcessingError;
    return file_data;
  }

  return file_data;
}

}  // namespace

void OmniboxPopupFileSelector::FileSelected(const ui::SelectedFileInfo& file,
                                            int index) {
  MultiFilesSelected({file});
}

void OmniboxPopupFileSelector::MultiFilesSelected(
    const std::vector<ui::SelectedFileInfo>& files) {
  ContextualSearchboxHandler* contextual_searchbox_handler =
      OmniboxContextMenuController::GetContextualSearchboxHandler(
          web_contents_.get());

  size_t valid_files_count =
      contextual_searchbox_handler
          ? contextual_searchbox_handler->GetUploadedContextTokens().size()
          : 0;

  size_t max_files = kDefaultMaxNumFiles;
  if (contextual_searchbox_handler) {
    auto composebox_config =
        ntp_composebox::FeatureConfig::Get().config.composebox();
    max_files = composebox_config.max_num_files();

    auto* input_state_model = contextual_searchbox_handler->input_state_model();
    if (input_state_model &&
        input_state_model->GetInputState().max_total_inputs > 0) {
      max_files = input_state_model->GetInputState().max_total_inputs;
    }
  }
  if (max_files == 0) {
    max_files = kDefaultMaxNumFiles;
  }

  bool has_posted_tasks = false;
  for (const auto& file : files) {
    if (valid_files_count >= max_files) {
      // To trigger the limit error banner on the WebUI frontend without reading
      // the exceeded file from disk or allocating memory for its contents, this
      // pushes a direct validation error to SearchboxContextData.
      // The frontend immediately displays the global limit error toast and
      // deletes this dummy context silently, rendering no failed chip.
      const std::string mime_type = GetMimeTypeForFile(
          file.path(), is_image_, /*allow_platform_lookup=*/false);
      contextual_search::ContextUploadErrorType error_type = contextual_search::
          ContextUploadErrorType::kBrowserProcessingMaxFilesExceededError;
      if (is_image_ || base::StartsWith(mime_type, "image/",
                                        base::CompareCase::INSENSITIVE_ASCII)) {
        error_type = contextual_search::ContextUploadErrorType::
            kBrowserProcessingMaxImagesExceededError;
      } else if (mime_type == "application/pdf") {
        error_type = contextual_search::ContextUploadErrorType::
            kBrowserProcessingMaxPdfsExceededError;
      }
      UpdateSearchboxContextData(lens::MimeType::kUnknown,
                                 /*image_data_url=*/"",
                                 file.path().BaseName().AsUTF8Unsafe(),
                                 /*mime_string=*/"application/octet-stream",
                                 base::unexpected(error_type));
      break;
    }
    valid_files_count++;
    has_posted_tasks = true;
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&ReadFileData, file.path(), GetMaxAttachmentSizeBytes(),
                       is_image_),
        base::BindOnce(&OmniboxPopupFileSelector::OnFileDataReady,
                       weak_factory_.GetWeakPtr()));
  }
  // This opens AI Mode however many of the files are valid, to show any
  // errors. If no file is read, it opens below. Otherwise `OnFileDataReady()`
  // opens it, even for a file that can't be attached.
  NotifyFileSelectionClosed(/*file_selected=*/true);
  deactivation_blocker_.reset();
  file_dialog_.reset();

  if (!has_posted_tasks) {
    OpenAiMode();
  }
}

void OmniboxPopupFileSelector::FileSelectionCanceled() {
  NotifyFileSelectionClosed(/*file_selected=*/false);
  deactivation_blocker_.reset();
  file_dialog_.reset();
  if (was_ai_mode_open_) {
    OpenAiMode();
  }
}

void OmniboxPopupFileSelector::OnFileDataReady(
    std::unique_ptr<FileData> file_data) {
  if (!file_data || !web_contents_) {
    if (was_ai_mode_open_) {
      OpenAiMode();
    }
    return;
  }
  const std::string file_name = file_data->path.BaseName().AsUTF8Unsafe();
  if (file_data->error.has_value()) {
    UpdateSearchboxContextData(lens::MimeType::kUnknown,
                               /*image_data_url=*/"", file_name,
                               file_data->mime_type,
                               base::unexpected(file_data->error.value()));
    OpenAiMode();
    return;
  }
  base::UmaHistogramExactLinear(
      "ContextualSearch.ContextAdded.ContextAddedMethod.Omnibox",
      /*ContextMenu*/ 0, 4);

  const std::string raw_mime_type = file_data->mime_type;

  lens::MimeType mime_type;
  if (raw_mime_type.find("pdf") != std::string::npos &&
      !lens::features::IsLensSendRawFileMediaTypesEnabled()) {
    mime_type = lens::MimeType::kPdf;
  } else if (raw_mime_type.find("image") != std::string::npos) {
    mime_type = lens::MimeType::kImage;
  } else if (lens::features::IsLensSendRawFileMediaTypesEnabled()) {
    // When raw file media types are enabled, all non-image files (including
    // pdfs) should be treated as generic files.
    mime_type = lens::MimeType::kUnknown;
  } else {
    UpdateSearchboxContextData(lens::MimeType::kUnknown, "", file_name,
                               raw_mime_type,
                               base::ok(base::UnguessableToken::Create()));
    OpenAiMode();
    return;
  }

  base::span<const uint8_t> file_data_span =
      base::as_bytes(base::span(file_data->bytes));
  std::vector<uint8_t> file_data_vector(file_data_span.begin(),
                                        file_data_span.end());
  mojo_base::BigBuffer file_data_buffer =
      mojo_base::BigBuffer(file_data_vector);

  std::string image_data_url;
  if (mime_type == lens::MimeType::kImage) {
    image_data_url = base::StrCat({"data:", raw_mime_type, ";base64,",
                                   base::Base64Encode(file_data->bytes)});
  }

  ContextualSearchboxHandler* contextual_searchbox_handler =
      OmniboxContextMenuController::GetContextualSearchboxHandler(
          web_contents_.get());
  if (contextual_searchbox_handler) {
    // Avoid accessing `file_data` directly in parameter expressions where
    // evaluation order is unspecified.
    contextual_searchbox_handler->AddFileContextFromBrowser(
        file_name, raw_mime_type, std::move(file_data_buffer),
        image_encoding_options_,
        base::BindOnce(&OmniboxPopupFileSelector::UpdateSearchboxContextData,
                       weak_factory_.GetWeakPtr(), mime_type,
                       std::move(image_data_url), file_name, raw_mime_type));
  }

  OpenAiMode();
  const std::string prefix = was_ai_mode_open_
                                 ? kAimContextTypeHistogramPrefix
                                 : kClassicContextTypeHistogramPrefix;
  const std::string sliced_prefix = base::StrCat({prefix, ".Clicked"});
  omnibox::ContextType context_type =
      is_image_ ? omnibox::ContextType::kImage : omnibox::ContextType::kFile;
  OmniboxContextMenuController::RecordContextMenuItemSelection(sliced_prefix,
                                                               context_type);
}

void OmniboxPopupFileSelector::UpdateSearchboxContextData(
    lens::MimeType mime_type,
    const std::string& image_data_url,
    const std::string& file_name,
    const std::string& mime_string,
    base::expected<base::UnguessableToken,
                   contextual_search::ContextUploadErrorType> result) {
  if (!web_contents_) {
    return;
  }
  OmniboxContextMenuController::AddFileContext(web_contents_.get(), mime_type,
                                               image_data_url, file_name,
                                               mime_string, result);
}

void OmniboxPopupFileSelector::NotifyFileSelectionClosed(bool file_selected) {
  if (file_chooser_closed_callback_) {
    file_chooser_closed_callback_.Run();
  }
  LocationBar* location_bar = GetLocationBar(web_contents_.get());
  if (!location_bar) {
    return;
  }
  if (was_ai_mode_open_) {
    if (auto* delegate = location_bar->GetPresenterDelegate()) {
      if (auto* presenter = delegate->GetOmniboxPopupAimPresenter()) {
        presenter->OnFileSelectionClosed();
      }
    }
  } else if (auto* popup_view = location_bar->GetOmniboxPopupView()) {
    if (OmniboxPopupHost* popup_host = popup_view->popup_host()) {
      // Blurs the omnibox while a dialog opened from the popup is shown, and
      // refocuses it once the dialog closes. See
      // `OmniboxPopupFullPresenter::OnFileDialogOpened()`.
      popup_host->OnFileDialogClosed(file_selected);
    }
  }
}

void OmniboxPopupFileSelector::OpenAiMode() {
  if (open_ai_mode_callback_) {
    open_ai_mode_callback_.Run();
  } else if (edit_model_) {
    edit_model_->OpenAiMode(OmniboxEditModel::AimActivation::kContextMenu);
  }
}
