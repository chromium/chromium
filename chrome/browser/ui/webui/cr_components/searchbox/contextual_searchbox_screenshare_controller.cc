// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_screenshare_controller.h"

#include <utility>

#include "build/build_config.h"
#include "content/public/browser/web_contents.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/media/webrtc/desktop_media_list.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_controller.h"
#include "chrome/browser/media/webrtc/desktop_media_picker_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/grit/branded_strings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_capture.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "skia/ext/image_operations.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/display/screen.h"
#include "ui/gfx/codec/png_codec.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace {

constexpr char kScreenshotFileName[] = "Screenshot.png";
constexpr char kScreenshotMimeType[] = "image/png";
constexpr int kMaxImageDimension = 2048;
constexpr int kMaxThumbnailDimension = 120;

SkBitmap DownscaleScreenshotBitmapIfNeeded(const SkBitmap& bitmap,
                                           int max_dimension) {
  if (bitmap.empty() ||
      (bitmap.width() <= max_dimension && bitmap.height() <= max_dimension)) {
    return bitmap;
  }

  gfx::Size scaled_size = lens::GetPreferredSize(
      gfx::Size(bitmap.width(), bitmap.height()), max_dimension, max_dimension);
  return skia::ImageOperations::Resize(
      bitmap, skia::ImageOperations::ResizeMethod::RESIZE_GOOD,
      scaled_size.width(), scaled_size.height());
}

ContextualSearchboxScreenshareController::ProcessedScreenshot
ProcessScreenshotInBackground(const SkBitmap& bitmap) {
  ContextualSearchboxScreenshareController::ProcessedScreenshot result;
  SkBitmap final_bitmap =
      DownscaleScreenshotBitmapIfNeeded(bitmap, kMaxImageDimension);

  std::optional<std::vector<uint8_t>> png_bytes =
      gfx::PNGCodec::EncodeBGRASkBitmap(final_bitmap,
                                        /*discard_transparency=*/false);
  if (!png_bytes) {
    return result;
  }
  result.png_bytes = std::move(*png_bytes);

  if (final_bitmap.width() <= kMaxThumbnailDimension &&
      final_bitmap.height() <= kMaxThumbnailDimension) {
    result.thumbnail_data_url = base::StrCat(
        {"data:image/png;base64,", base::Base64Encode(result.png_bytes)});
    return result;
  }

  SkBitmap thumbnail_bitmap =
      DownscaleScreenshotBitmapIfNeeded(final_bitmap, kMaxThumbnailDimension);
  std::optional<std::vector<uint8_t>> thumbnail_png_bytes =
      gfx::PNGCodec::EncodeBGRASkBitmap(thumbnail_bitmap,
                                        /*discard_transparency=*/false);
  if (thumbnail_png_bytes) {
    result.thumbnail_data_url = base::StrCat(
        {"data:image/png;base64,", base::Base64Encode(*thumbnail_png_bytes)});
  }
  return result;
}

std::optional<lens::ImageEncodingOptions> CreateImageEncodingOptions() {
  const auto& image_upload_config =
      ntp_composebox::FeatureConfig::Get().config.composebox().image_upload();
  return lens::ImageEncodingOptions{
      .max_size = image_upload_config.downscale_max_image_size(),
      .max_height = image_upload_config.downscale_max_image_height(),
      .max_width = image_upload_config.downscale_max_image_width(),
      .compression_quality = image_upload_config.image_compression_quality()};
}

}  // namespace
#endif  // !BUILDFLAG(IS_ANDROID)

ContextualSearchboxScreenshareController::
    ContextualSearchboxScreenshareController(
        content::WebContents* web_contents,
        Host* host,
        Delegate* delegate,
        DesktopMediaPickerFactory* picker_factory)
    : web_contents_(web_contents),
      host_(host),
      delegate_(delegate)
#if !BUILDFLAG(IS_ANDROID)
      ,
      picker_factory_(picker_factory)
#endif
{
}

ContextualSearchboxScreenshareController::
    ~ContextualSearchboxScreenshareController() {
#if !BUILDFLAG(IS_ANDROID)
  if (IsScreenshareInProgress()) {
    NotifyScreensharePickerClosed();
  }
  if (pending_screenshare_callback_) {
    std::move(pending_screenshare_callback_).Run(std::nullopt);
  }
#endif
}

void ContextualSearchboxScreenshareController::ShowScreenshotMenu(
    const gfx::Rect& anchor_rect) {
  if (delegate_) {
    delegate_->ShowScreenshotMenu(anchor_rect, weak_ptr_factory_.GetWeakPtr());
  } else {
    OnScreenshotMenuClosed();
  }
}

void ContextualSearchboxScreenshareController::OnScreenshotMenuClosed() {
  if (host_) {
    host_->OnScreenshotMenuClosed();
  }
}

void ContextualSearchboxScreenshareController::StartScreenshare(
    bool prefer_entire_screen,
    StartScreenshareCallback callback) {
#if !BUILDFLAG(IS_ANDROID)
  StartScreenshareInternal(prefer_entire_screen, /*is_region_capture=*/false,
                           std::move(callback));
#else
  std::move(callback).Run(std::nullopt);
#endif
}

void ContextualSearchboxScreenshareController::CaptureRegionScreenshot(
    CaptureRegionScreenshotCallback callback) {
#if !BUILDFLAG(IS_ANDROID)
  StartScreenshareInternal(/*prefer_entire_screen=*/true,
                           /*is_region_capture=*/true, std::move(callback));
#else
  std::move(callback).Run(std::nullopt);
#endif
}

#if !BUILDFLAG(IS_ANDROID)
void ContextualSearchboxScreenshareController::StartScreenshareInternal(
    bool prefer_entire_screen,
    bool is_region_capture,
    StartScreenshareCallback callback) {
  if (IsScreenshareInProgress()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  bool use_native_picker = false;
#if BUILDFLAG(IS_MAC)
  if (base::mac::MacOSMajorVersion() >= 14) {
    use_native_picker =
        base::FeatureList::IsEnabled(media::kUseSCContentSharingPicker);
  }
#endif

  auto safe_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), std::nullopt);
  if (!use_native_picker) {
#if BUILDFLAG(IS_MAC)
    FallbackToChromeDefaultPicker(prefer_entire_screen, is_region_capture,
                                  std::move(safe_callback));
#else
    if (is_region_capture) {
      CaptureFullDesktopRegionScreenshot(std::move(safe_callback));
    } else {
      FallbackToChromeDefaultPicker(prefer_entire_screen,
                                    /*is_region_capture=*/false,
                                    std::move(safe_callback));
    }
#endif
    return;
  }

  content::DesktopMediaID::Type target_capture_type =
      prefer_entire_screen ? content::DesktopMediaID::TYPE_SCREEN
                           : content::DesktopMediaID::TYPE_WINDOW;

  auto [picker_selected_callback, remaining_callback] =
      base::SplitOnceCallback(std::move(safe_callback));
  auto [picker_cancelled_callback, fallback_callback] =
      base::SplitOnceCallback(std::move(remaining_callback));

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &content::desktop_capture::OpenNativeScreenCapturePicker,
          target_capture_type,
          base::BindPostTask(
              content::GetUIThreadTaskRunner({}),
              base::BindOnce(&ContextualSearchboxScreenshareController::
                                 OnNativePickerCreated,
                             weak_ptr_factory_.GetWeakPtr())),
          base::BindPostTask(
              content::GetUIThreadTaskRunner({}),
              base::BindOnce(&ContextualSearchboxScreenshareController::
                                 OnNativePickerSourceSelected,
                             weak_ptr_factory_.GetWeakPtr(),
                             target_capture_type, is_region_capture,
                             std::move(picker_selected_callback))),
          base::BindPostTask(
              content::GetUIThreadTaskRunner({}),
              base::BindOnce(&ContextualSearchboxScreenshareController::
                                 OnNativePickerCancelled,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(picker_cancelled_callback))),
          base::BindPostTask(
              content::GetUIThreadTaskRunner({}),
#if BUILDFLAG(IS_MAC)
              base::BindOnce(&ContextualSearchboxScreenshareController::
                                 FallbackToChromeDefaultPicker,
                             weak_ptr_factory_.GetWeakPtr(),
                             prefer_entire_screen, is_region_capture,
                             std::move(fallback_callback))
#else
              is_region_capture
                  ? base::BindOnce(&ContextualSearchboxScreenshareController::
                                       CaptureFullDesktopRegionScreenshot,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(fallback_callback))
                  : base::BindOnce(&ContextualSearchboxScreenshareController::
                                       FallbackToChromeDefaultPicker,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   prefer_entire_screen,
                                   /*is_region_capture=*/false,
                                   std::move(fallback_callback))
#endif
                  )));
}

void ContextualSearchboxScreenshareController::
    CaptureFullDesktopRegionScreenshot(
        CaptureRegionScreenshotCallback callback) {
  if (IsScreenshareInProgress()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  NotifyScreensharePickerOpened();
  // Captures the full virtual desktop across all connected monitors
  // (webrtc::kFullDesktopScreenId = -1).
  constexpr content::DesktopMediaID::Id kFullDesktopScreenId = -1;
  content::DesktopMediaID source(content::DesktopMediaID::TYPE_SCREEN,
                                 kFullDesktopScreenId);
  CaptureAndUploadScreenshot(source, std::move(callback),
                             RegionCaptureSource::AllDisplays());
}

void ContextualSearchboxScreenshareController::OnNativePickerCreated(
    content::DesktopMediaID::Id /*session_id*/) {
  is_native_picker_open_ = true;
  NotifyScreensharePickerOpened();
}

void ContextualSearchboxScreenshareController::OnNativePickerSourceSelected(
    content::DesktopMediaID::Type capture_type,
    bool is_region_capture,
    StartScreenshareCallback callback,
    webrtc::DesktopCapturer::Source selected_source) {
  is_native_picker_open_ = false;
  content::DesktopMediaID media_id(capture_type, selected_source.id);
#if BUILDFLAG(IS_MAC)
  media_id.id_type = content::DesktopMediaID::IdType::kNativePickerSession;
#endif

  std::optional<RegionCaptureSource> region_source;
  if (is_region_capture) {
    CHECK_NE(selected_source.display_id, webrtc::kInvalidDisplayId);
    region_source = RegionCaptureSource::ForDisplay(selected_source.display_id);
  }

  CaptureAndUploadScreenshot(media_id, std::move(callback), region_source);
}

void ContextualSearchboxScreenshareController::OnNativePickerCancelled(
    StartScreenshareCallback callback) {
  is_native_picker_open_ = false;
  NotifyScreensharePickerClosed();
  std::move(callback).Run(std::nullopt);
}

void ContextualSearchboxScreenshareController::FallbackToChromeDefaultPicker(
    bool prefer_entire_screen,
    bool is_region_capture,
    StartScreenshareCallback callback) {
  is_native_picker_open_ = false;
  if (IsScreenshareInProgress()) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  chrome_default_picker_destroyed_ = false;
  pending_screenshare_source_.reset();
  pending_screenshare_callback_.Reset();
  pending_region_capture_source_.reset();
  screenshare_picker_controller_ =
      std::make_unique<DesktopMediaPickerController>(picker_factory_);

  DesktopMediaPicker::Params picker_params(
      DesktopMediaPicker::Params::RequestSource::kSearchbox);
  picker_params.web_contents = nullptr;
  picker_params.includable_web_contents_filter =
      base::BindRepeating([](content::WebContents*) { return true; });

  gfx::NativeWindow parent_window = gfx::NativeWindow();
  if (web_contents_) {
    auto* browser_window = webui::GetBrowserWindowInterface(web_contents_);
    if (browser_window && browser_window->GetWindow()) {
      parent_window = browser_window->GetWindow()->GetNativeWindow();
    } else {
      parent_window = web_contents_->GetTopLevelNativeWindow();
    }
  }

  picker_params.context = parent_window;
  picker_params.parent = parent_window;
  picker_params.app_name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  picker_params.target_name = picker_params.app_name;
  picker_params.modality = ui::mojom::ModalType::kWindow;

  std::vector<DesktopMediaList::Type> sources;
  if (is_region_capture) {
    sources = {DesktopMediaList::Type::kScreen};
  } else {
    sources = {DesktopMediaList::Type::kScreen,
               DesktopMediaList::Type::kWindow};
  }
  picker_params.preferred_display_surface =
      prefer_entire_screen ? blink::mojom::PreferredDisplaySurface::MONITOR
                           : blink::mojom::PreferredDisplaySurface::WINDOW;
  picker_params.on_picker_destroying = base::BindRepeating(
      &ContextualSearchboxScreenshareController::OnChromeDefaultPickerDestroyed,
      weak_ptr_factory_.GetWeakPtr());

  screenshare_picker_controller_->Show(
      picker_params, sources,
      base::BindOnce(&ContextualSearchboxScreenshareController::
                         OnChromeDefaultPickerResults,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     is_region_capture),
      base::BindOnce(&ContextualSearchboxScreenshareController::
                         NotifyScreensharePickerOpened,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ContextualSearchboxScreenshareController::OnChromeDefaultPickerResults(
    StartScreenshareCallback callback,
    bool is_region_capture,
    const std::string& err,
    content::DesktopMediaID source) {
  screenshare_picker_controller_.reset();
  if (source.is_null()) {
    chrome_default_picker_destroyed_ = false;
    NotifyScreensharePickerClosed();
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::optional<RegionCaptureSource> region_source;
  if (is_region_capture) {
    region_source = RegionCaptureSource::ForDisplay(source.id);
  }
  if (chrome_default_picker_destroyed_) {
    chrome_default_picker_destroyed_ = false;
    CaptureAndUploadScreenshot(source, std::move(callback), region_source);
  } else {
    pending_screenshare_source_ = source;
    pending_screenshare_callback_ = std::move(callback);
    pending_region_capture_source_ = region_source;
  }
}

void ContextualSearchboxScreenshareController::
    OnChromeDefaultPickerDestroyed() {
  if (pending_screenshare_callback_) {
    chrome_default_picker_destroyed_ = false;
    content::DesktopMediaID source = pending_screenshare_source_.value();
    pending_screenshare_source_.reset();
    auto callback = std::move(pending_screenshare_callback_);
    std::optional<RegionCaptureSource> region_source =
        std::move(pending_region_capture_source_);
    pending_region_capture_source_.reset();
    CaptureAndUploadScreenshot(source, std::move(callback), region_source);
  } else {
    chrome_default_picker_destroyed_ = true;
  }
}

void ContextualSearchboxScreenshareController::CaptureAndUploadScreenshot(
    content::DesktopMediaID source,
    StartScreenshareCallback callback,
    std::optional<RegionCaptureSource> region_capture_source) {
  is_capturing_ = true;
  auto safe_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      std::move(callback), std::nullopt);
  active_screenshot_request_ = content::desktop_capture::CaptureScreenshot(
      source,
      base::BindOnce(
          &ContextualSearchboxScreenshareController::OnScreenshotCaptured,
          weak_ptr_factory_.GetWeakPtr(), std::move(safe_callback),
          std::move(region_capture_source)));
  if (!active_screenshot_request_) {
    NotifyScreensharePickerClosed();
    is_capturing_ = false;
  }
}

void ContextualSearchboxScreenshareController::OnScreenshotCaptured(
    StartScreenshareCallback callback,
    std::optional<RegionCaptureSource> region_capture_source,
    const SkBitmap& bitmap) {
  active_screenshot_request_.reset();
  if (!region_capture_source) {
    NotifyScreensharePickerClosed();
  }
  if (bitmap.empty()) {
    if (region_capture_source) {
      NotifyScreensharePickerClosed();
    }
    is_capturing_ = false;
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (region_capture_source) {
    if (delegate_) {
      is_region_overlay_open_ = true;
      delegate_->ShowRegionSelectOverlay(
          bitmap, *region_capture_source,
          base::BindOnce(
              &ContextualSearchboxScreenshareController::OnRegionSelected,
              weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    } else {
      NotifyScreensharePickerClosed();
      is_capturing_ = false;
      std::move(callback).Run(std::nullopt);
    }
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ProcessScreenshotInBackground, bitmap),
      base::BindOnce(
          &ContextualSearchboxScreenshareController::OnScreenshotProcessed,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ContextualSearchboxScreenshareController::OnRegionSelected(
    StartScreenshareCallback callback,
    const SkBitmap& region_bitmap) {
  is_region_overlay_open_ = false;
  NotifyScreensharePickerClosed();
  if (region_bitmap.empty()) {
    is_capturing_ = false;
    std::move(callback).Run(std::nullopt);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ProcessScreenshotInBackground, region_bitmap),
      base::BindOnce(
          &ContextualSearchboxScreenshareController::OnScreenshotProcessed,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ContextualSearchboxScreenshareController::OnScreenshotProcessed(
    StartScreenshareCallback callback,
    ProcessedScreenshot result) {
  is_capturing_ = false;
  if (result.png_bytes.empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  auto file_info_mojom = searchbox::mojom::SelectedFileInfo::New();
  file_info_mojom->file_name = kScreenshotFileName;
  file_info_mojom->mime_type = kScreenshotMimeType;
  file_info_mojom->is_deletable = true;
  file_info_mojom->selection_time = base::Time::Now();
  file_info_mojom->image_data_url = result.thumbnail_data_url;

  mojo_base::BigBuffer file_bytes(std::move(result.png_bytes));
  if (host_) {
    host_->UploadScreenshot(
        kScreenshotFileName, kScreenshotMimeType, std::move(file_bytes),
        /*image_encoding_options=*/CreateImageEncodingOptions(),
        base::BindOnce(
            [](StartScreenshareCallback callback,
               base::WeakPtr<ContextualSearchboxScreenshareController>
                   controller,
               searchbox::mojom::SelectedFileInfoPtr file_info_mojom,
               base::expected<base::UnguessableToken,
                              contextual_search::ContextUploadErrorType>
                   result) {
              std::optional<base::UnguessableToken> token = std::nullopt;
              if (result.has_value() && controller && controller->host_) {
                token = result.value();
                controller->host_->AddFileContextToPage(
                    result.value(), std::move(file_info_mojom));
              }
              std::move(callback).Run(token);
            },
            std::move(callback), weak_ptr_factory_.GetWeakPtr(),
            std::move(file_info_mojom)));
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

void ContextualSearchboxScreenshareController::NotifyScreensharePickerOpened() {
  if (delegate_) {
    delegate_->OnScreensharePickerOpened();
  }
}

void ContextualSearchboxScreenshareController::NotifyScreensharePickerClosed() {
  if (delegate_) {
    delegate_->OnScreensharePickerClosed();
  }
}

bool ContextualSearchboxScreenshareController::IsScreenshareInProgress() const {
  return screenshare_picker_controller_ || is_capturing_ ||
         is_native_picker_open_ || is_region_overlay_open_ ||
         pending_screenshare_callback_;
}
#endif  // !BUILDFLAG(IS_ANDROID)
