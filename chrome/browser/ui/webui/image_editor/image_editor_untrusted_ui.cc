// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/image_editor/image_editor_untrusted_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_piece.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/image_editor/image_editor_component_info.h"
#include "chrome/browser/image_editor/screenshot_flow.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/image_editor/image_editor.mojom.h"
#include "chrome/browser/ui/webui/image_editor/image_editor_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/image_editor_untrusted_resources.h"
#include "chrome/grit/image_editor_untrusted_resources_map.h"
#include "components/component_updater/component_updater_paths.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/webui/untrusted_web_ui_controller.h"
#include "url/gurl.h"

namespace image_editor {

constexpr char kFilenameScreenshotPng[] = "screenshot.png";

#if !defined(OFFICIAL_BUILD)
// Set to true and provide a string path to load resources from disk.
// As the WebUI is loaded via component, this allows for easier local
// development.
constexpr bool kLoadFromLocalFileForDevelopment = false;
constexpr base::FilePath::CharType kLocalDebugPath[] = FILE_PATH_LITERAL("");
#endif

ImageEditorUntrustedUIConfig::ImageEditorUntrustedUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  chrome::kChromeUIImageEditorHost) {}

ImageEditorUntrustedUIConfig::~ImageEditorUntrustedUIConfig() = default;

std::unique_ptr<content::WebUIController>
ImageEditorUntrustedUIConfig::CreateWebUIController(content::WebUI* web_ui) {
  return std::make_unique<ImageEditorUntrustedUI>(web_ui);
}

ImageEditorUntrustedUI::ImageEditorUntrustedUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  Profile* profile = Profile::FromWebUI(web_ui);
  if (!profile) {
    return;
  }
  image_editor::ScreenshotCapturedData* screenshot_data =
      static_cast<image_editor::ScreenshotCapturedData*>(
          profile->GetUserData(image_editor::ScreenshotCapturedData::kDataKey));
  if (screenshot_data) {
    screenshot_filepath_ = screenshot_data->screenshot_filepath;
  }

  CreateAndAddImageEditorUntrustedDataSource(web_ui);
}

ImageEditorUntrustedUI::~ImageEditorUntrustedUI() = default;

void ImageEditorUntrustedUI::BindInterface(
    mojo::PendingReceiver<mojom::ImageEditorHandler> pending_receiver) {
  // TODO(crbug.com/1297362): The lifetime of the WebUIController and the mojo
  // interface may vary. This requires supporting multiple binding.
  if (receiver_.is_bound())
    receiver_.reset();
  receiver_.Bind(std::move(pending_receiver));
}

void ImageEditorUntrustedUI::RecordUserAction(mojom::EditAction action) {
  base::UmaHistogramEnumeration("Sharing.DesktopScreenshot.Action", action);
}

void FinishResourceLoad(
    content::WebUIDataSource::GotDataCallback got_data_callback,
    absl::optional<std::string> data) {
  if (!data.has_value()) {
    std::move(got_data_callback).Run(nullptr);
    return;
  }

  std::move(got_data_callback)
      .Run(base::MakeRefCounted<base::RefCountedBytes>(
          reinterpret_cast<const unsigned char*>(data->data()), data->size()));
}

void ImageEditorUntrustedUI::StartLoadFromComponentLoadBytes(
    const std::string& resource_path,
    content::WebUIDataSource::GotDataCallback got_data_callback) {
  base::FilePath filepath;
  if (resource_path == kFilenameScreenshotPng) {
    // screenshot.png is provided from the image capture process.
    if (screenshot_filepath_.empty()) {
      std::move(got_data_callback).Run(nullptr);
      return;
    }
    filepath = screenshot_filepath_;
  }

  // Other requests passed to this filter are loaded from the component,
  // possibly overridden by a local debug directory in non-official builds.
#if !defined(OFFICIAL_BUILD)
  if constexpr (kLoadFromLocalFileForDevelopment) {
    if (filepath.empty()) {
      filepath = base::FilePath(kLocalDebugPath).AppendASCII(resource_path);
    }
  }
#endif

  if (filepath.empty()) {
    filepath =
        ImageEditorComponentInfo::GetInstance()->GetInstalledPath().AppendASCII(
            resource_path);
  }

  auto load_file = base::BindOnce(
      [](const base::FilePath& filepath) -> absl::optional<std::string> {
        std::string contents;
        return base::ReadFileToString(filepath, &contents)
                   ? absl::make_optional<std::string>(contents)
                   : absl::nullopt;
      },
      filepath);

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, std::move(load_file),
      base::BindOnce(&FinishResourceLoad, std::move(got_data_callback)));
}

void AddLocalizedStrings(content::WebUIDataSource* source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"Screenshot", IDS_SHARING_HUB_SCREENSHOT_LABEL},
      {"Tooltip-Select",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_SELECTION},
      {"Tooltip-Crop",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_CROP},
      {"Tooltip-Text",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_TEXT},
      {"Tooltip-Ellipse",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_ELLIPSE},
      {"Tooltip-Rectangle",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_RECTANGLE},
      {"Tooltip-Line",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_LINE},
      {"Tooltip-Arrow",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_ARROW},
      {"Tooltip-Brush",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_BRUSH},
      {"Tooltip-Emoji",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_EMOJI},
      {"Tooltip-Highlighter",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_HIGHLIGHTER},
      {"Tooltip-Undo",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_UNDO},
      {"Tooltip-Redo",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_REDO},
      {"Tooltip-ZoomIn",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_ZOOM_IN},
      {"Tooltip-ZoomOut",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_ZOOM_OUT},
      {"Button-Commit-Crop",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_COMMIT_CROP},
      {"Tooltip-Commit-Crop",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_COMMIT_CROP},
      {"Button-Cancel-Crop",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_CANCEL_CROP},
      {"Tooltip-Cancel-Crop",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_CANCEL_CROP},
      {"Button-Download-Image",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_BUTTON_DOWNLOAD_IMAGE},
      {"Button-Copy-Image",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_BUTTON_COPY},
      {"Title", IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TITLE},
      {"Button-Clear",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_BUTTON_TEXT_CLEAR},
      {"Tooltip-Align-Left",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_ALIGN_LEFT},
      {"Tooltip-Align-Center",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_ALIGN_CENTER},
      {"Tooltip-Align-Right",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_ALIGN_RIGHT},
      {"Tooltip-Shadow-None",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_SHADOW_NONE},
      {"Tooltip-Shadow-Small",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_SHADOW_SMALL},
      {"Tooltip-Shadow-Large",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_SHADOW_LARGE},
      {"Tooltip-Font-Default",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_FONT_DEFAULT},
      {"Tooltip-Font-Monospace",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_FONT_MONOSPACE},
      {"Tooltip-Font-Italic",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_FONT_ITALIC},
      {"Tooltip-Font-Outline",
       IDS_BROWSER_SHARING_SCREENSHOT_IMAGE_EDITOR_TOOLTIP_FONT_OUTLINE},
  };
  source->AddLocalizedStrings(kLocalizedStrings);
}

bool IsFileProvidedByRequestFilter(const std::string& path) {
  // Mojo interface is obtained from resources.
  for (size_t i = 0; i < kImageEditorUntrustedResourcesSize; ++i) {
    if (path == kImageEditorUntrustedResources[i].path)
      return false;
  }
  return true;
}

content::WebUIDataSource*
ImageEditorUntrustedUI::CreateAndAddImageEditorUntrustedDataSource(
    content::WebUI* web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIUntrustedImageEditorURL);

  // Add translated strings.
  AddLocalizedStrings(source);
  source->UseStringsJs();

  // Provide Mojo .js files.
  source->AddResourcePaths(base::make_span(kImageEditorUntrustedResources,
                                           kImageEditorUntrustedResourcesSize));

  // The majority of files are loaded from a component.
  source->SetRequestFilter(
      base::BindRepeating(&IsFileProvidedByRequestFilter),
      base::BindRepeating(
          &ImageEditorUntrustedUI::StartLoadFromComponentLoadBytes,
          weak_factory_.GetWeakPtr()));

  // Specify that the chrome-untrusted://image-editor iframe is embedded in the
  // chrome://image-editor host.
  source->AddFrameAncestor(GURL(chrome::kChromeUIImageEditorURL));

  // By default, prevent all network access.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc,
      "default-src blob: 'self';");

  // For Ink:
  // Need to explicitly set |worker-src| because CSP falls back to |child-src|
  // which is none.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src 'self';");
  // Allow images to also handle data urls.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc, "img-src blob: data: 'self';");
  // Allow styles to include inline styling needed for some Lit components,
  // and used on the top-level elements.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' 'unsafe-inline';");
  // Allow wasm.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src 'self' 'wasm-eval';");
  // Allow use of SharedArrayBuffer (required by wasm).
  source->OverrideCrossOriginOpenerPolicy("same-origin");
  source->OverrideCrossOriginEmbedderPolicy("require-corp");

  // Allow chrome://image-editor to load chrome-untrusted://index.html in
  // an iframe.
  source->OverrideCrossOriginResourcePolicy("cross-origin");

  // TODO(crbug.com/1098685, crbug.com/1314850): Trusted Type remaining WebUI.
  source->DisableTrustedTypesCSP();

  return source;
}

WEB_UI_CONTROLLER_TYPE_IMPL(ImageEditorUntrustedUI)

}  // namespace image_editor
