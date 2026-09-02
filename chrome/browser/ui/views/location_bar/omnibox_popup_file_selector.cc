// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/contextual_search/searchbox_context_data.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_context_menu_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_state_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_view.h"
#include "chrome/browser/ui/select_file_policy/chrome_select_file_policy.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"
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
#include "components/omnibox/common/input_state.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/omnibox/common/omnibox_metrics_utils.h"
#include "content/public/browser/web_contents.h"
#include "net/base/mime_util.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"

OmniboxPopupFileSelector::OmniboxPopupFileSelector(
    gfx::NativeWindow owning_window)
    : owning_window_(owning_window) {}

OmniboxPopupFileSelector::~OmniboxPopupFileSelector() = default;

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
  web_contents_ = web_contents ? web_contents->GetWeakPtr() : nullptr;
  edit_model_ = edit_model;
  image_encoding_options_ = image_encoding_options;
  was_ai_mode_open_ = was_ai_mode_open;
  is_image_ = is_image;
  if (file_chooser_opened_callback_) {
    file_chooser_opened_callback_.Run();
  }
  if (web_contents) {
    if (auto* browser_window = webui::GetBrowserWindowInterface(web_contents)) {
      if (auto* location_bar = browser_window->GetFeatures().location_bar()) {
        if (was_ai_mode_open) {
          if (auto* presenter_delegate = location_bar->GetPresenterDelegate()) {
            if (auto* presenter =
                    presenter_delegate->GetOmniboxPopupAimPresenter()) {
              deactivation_blocker_ = presenter->CreateDeactivationBlocker();
            }
          }
        } else {
          if (auto* popup_view = location_bar->GetOmniboxPopupView()) {
            if (auto* presenter = popup_view->presenter()) {
              deactivation_blocker_ = presenter->CreateDeactivationBlocker();
            }
          }
        }
      }
    }
  }

  if (file_dialog_) {
    file_dialog_->ListenerDestroyed();
    file_dialog_.reset();
  }

  file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));

  const ui::SelectFileDialog::Type dialog_type =
      ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE;

  ui::SelectFileDialog::FileTypeInfo file_types;
  if (is_image) {
    std::vector<base::FilePath::StringType> extensions;
    net::GetExtensionsForMimeType("image/*", &extensions);
    file_types.extensions.push_back(extensions);
  } else if (!lens::features::IsLensSendRawFileMediaTypesEnabled()) {
    file_types.extensions = {{FILE_PATH_LITERAL("pdf")}};
  }

  file_types.include_all_files = true;

  file_dialog_->SelectFile(dialog_type, /*title=*/u"", base::FilePath(),
                           &file_types, 0, base::FilePath::StringType(),
                           owning_window_);
}

namespace {

constexpr size_t kDefaultMaxNumFiles = omnibox::kDefaultMaxTotalInputs;

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

bool IsImageFile(const base::FilePath& path) {
  return path.MatchesExtension(FILE_PATH_LITERAL(".png")) ||
         path.MatchesExtension(FILE_PATH_LITERAL(".jpg")) ||
         path.MatchesExtension(FILE_PATH_LITERAL(".jpeg")) ||
         path.MatchesExtension(FILE_PATH_LITERAL(".webp")) ||
         path.MatchesExtension(FILE_PATH_LITERAL(".gif")) ||
         path.MatchesExtension(FILE_PATH_LITERAL(".bmp"));
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
      contextual_search::ContextUploadErrorType error_type =
          contextual_search::ContextUploadErrorType::
              kBrowserProcessingMaxFilesExceededError;
      if (is_image_ || IsImageFile(file.path())) {
        error_type = contextual_search::ContextUploadErrorType::
            kBrowserProcessingMaxImagesExceededError;
      } else if (file.path().MatchesExtension(FILE_PATH_LITERAL(".pdf"))) {
        error_type = contextual_search::ContextUploadErrorType::
            kBrowserProcessingMaxPdfsExceededError;
      }
      UpdateSearchboxContextData(
          lens::MimeType::kUnknown, /*image_data_url=*/"",
          file.path().BaseName().AsUTF8Unsafe(),
          /*mime_string=*/"application/octet-stream",
          base::unexpected(error_type));
      break;
    }
    valid_files_count++;
    has_posted_tasks = true;
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&ReadFileAndProcess, file.path()),
        base::BindOnce(&OmniboxPopupFileSelector::OnFileDataReady,
                       weak_factory_.GetWeakPtr()));
  }
  NotifyFileSelectionClosed();
  deactivation_blocker_.reset();
  file_dialog_.reset();

  if (!has_posted_tasks) {
    OpenAiMode();
  }
}

void OmniboxPopupFileSelector::FileSelectionCanceled() {
  NotifyFileSelectionClosed();
  deactivation_blocker_.reset();
  file_dialog_.reset();
  if (was_ai_mode_open_) {
    OpenAiMode();
  }
}

void OmniboxPopupFileSelector::OnFileDataReady(
    std::unique_ptr<FileData> file_data) {
  if (!file_data || !web_contents_) {
    return;
  }
  base::UmaHistogramExactLinear(
      "ContextualSearch.ContextAdded.ContextAddedMethod.Omnibox",
      /*ContextMenu*/ 0, 4);

  lens::MimeType mime_type;
  if (file_data->mime_type.find("pdf") != std::string::npos &&
      !lens::features::IsLensSendRawFileMediaTypesEnabled()) {
    mime_type = lens::MimeType::kPdf;
  } else if (file_data->mime_type.find("image") != std::string::npos) {
    mime_type = lens::MimeType::kImage;
  } else if (lens::features::IsLensSendRawFileMediaTypesEnabled()) {
    // When raw file media types are enabled, all non-image files (including
    // pdfs) should be treated as generic files.
    mime_type = lens::MimeType::kUnknown;
  } else {
    UpdateSearchboxContextData(lens::MimeType::kUnknown, "", file_data->name,
                               file_data->mime_type,
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
    image_data_url = base::StrCat({"data:", file_data->mime_type, ";base64,",
                                   base::Base64Encode(file_data->bytes)});
  }

  ContextualSearchboxHandler* contextual_searchbox_handler =
      OmniboxContextMenuController::GetContextualSearchboxHandler(
          web_contents_.get());
  if (contextual_searchbox_handler) {
    // The order of execution of parameter evaluation is undefined in C++. On
    // Windows this results in `file_data->mime_type` being moved before the
    // other mime_type use is evaluated resulting. The move results in an
    // empty mime_type value being passed into `AddFileContextFromBrowser`.
    // See: https://en.cppreference.com/w/cpp/language/eval_order and
    // http://crbug.com/472510275.
    std::string mime_type_copy = file_data->mime_type;
    std::string file_name_copy = file_data->name;
    contextual_searchbox_handler->AddFileContextFromBrowser(
        std::move(file_name_copy), std::move(mime_type_copy),
        std::move(file_data_buffer), image_encoding_options_,
        base::BindOnce(&OmniboxPopupFileSelector::UpdateSearchboxContextData,
                       weak_factory_.GetWeakPtr(), mime_type,
                       std::move(image_data_url), file_data->name,
                       file_data->mime_type));
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

  if (OmniboxContextMenuController::GetOmniboxEverywhereUI(
          web_contents_.get())) {
    if (auto* handler =
            OmniboxContextMenuController::GetContextualSearchboxHandler(
                web_contents_.get())) {
      auto file_info_mojom = searchbox::mojom::SelectedFileInfo::New();
      file_info_mojom->file_name = file_name;
      file_info_mojom->mime_type = mime_string;
      file_info_mojom->is_deletable = true;
      file_info_mojom->selection_time = base::Time::Now();
      if (mime_type == lens::MimeType::kImage) {
        file_info_mojom->image_data_url = image_data_url;
      }
      if (!result.has_value()) {
        base::UnguessableToken error_token = base::UnguessableToken::Create();
        handler->AddFileContextFromBrowser(error_token,
                                           std::move(file_info_mojom));
        handler->OnContextUploadStatusChanged(
            error_token, mime_type,
            contextual_search::ContextUploadStatus::kValidationFailed,
            result.error());
      } else {
        handler->AddFileContextFromBrowser(result.value(),
                                           std::move(file_info_mojom));
      }
    }
    return;
  }

  auto file_attachment = searchbox::mojom::FileAttachment::New();
  file_attachment->uuid =
      result.has_value() ? result.value() : base::UnguessableToken::Create();
  file_attachment->name = file_name;
  file_attachment->mime_type = mime_string;
  if (!result.has_value()) {
    file_attachment->error_type = result.error();
  }
  if (mime_type == lens::MimeType::kImage) {
    file_attachment->image_data_url = image_data_url;
  }
  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_contents_.get());
  if (!browser_window_interface) {
    return;
  }
  SearchboxContextData* searchbox_context_data =
      SearchboxContextData::From(browser_window_interface);
  if (!searchbox_context_data) {
    return;
  }
  auto context = searchbox_context_data->TakePendingContext();
  if (!context) {
    context = std::make_unique<SearchboxContextData::Context>();
  }
  context->file_infos.push_back(
      searchbox::mojom::SearchContextAttachment::NewFileAttachment(
          std::move(file_attachment)));
  auto* location_bar = browser_window_interface->GetFeatures().location_bar();
  auto* omnibox_controller =
      location_bar ? location_bar->GetOmniboxController() : nullptr;
  if (omnibox_controller && omnibox_controller->popup_state_manager() &&
      omnibox_controller->popup_state_manager()->popup_state() ==
          OmniboxPopupState::kAim) {
    if (auto* webui = web_contents_->GetWebUI()) {
      auto* omnibox_popup_ui = webui->GetController()->GetAs<OmniboxPopupUI>();
      if (omnibox_popup_ui && omnibox_popup_ui->popup_aim_handler()) {
        omnibox_popup_ui->popup_aim_handler()->AddContext(std::move(context));
      }
    }
  } else {
    searchbox_context_data->SetPendingContext(std::move(context));
  }
}

void OmniboxPopupFileSelector::NotifyFileSelectionClosed() {
  if (file_chooser_closed_callback_) {
    file_chooser_closed_callback_.Run();
  }
  if (was_ai_mode_open_ && web_contents_) {
    auto* browser_window =
        webui::GetBrowserWindowInterface(web_contents_.get());
    if (!browser_window) {
      return;
    }

    auto* location_bar = browser_window->GetFeatures().location_bar();
    if (!location_bar) {
      return;
    }

    auto* presenter_delegate = location_bar->GetPresenterDelegate();
    if (!presenter_delegate) {
      return;
    }

    if (auto* presenter = presenter_delegate->GetOmniboxPopupAimPresenter()) {
      presenter->OnFileSelectionClosed();
    }
  }
}

void OmniboxPopupFileSelector::OpenAiMode() {
  if (edit_model_) {
    edit_model_->OpenAiMode(OmniboxEditModel::AimActivation::kContextMenu);
  } else if (open_ai_mode_callback_) {
    open_ai_mode_callback_.Run();
  }
}
