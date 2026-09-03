// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_CONTEXTUAL_SEARCHBOX_SCREENSHARE_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_CONTEXTUAL_SEARCHBOX_SCREENSHARE_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere_service.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/lens/lens_bitmap_processing.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"

#if !BUILDFLAG(IS_ANDROID)
#include "content/public/browser/desktop_media_id.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#endif

namespace content {
class WebContents;
namespace desktop_capture {
class ScreenshotCaptureRequest;
}
}  // namespace content

class DesktopMediaPickerController;
class DesktopMediaPickerFactory;

// Dedicated controller managing screenshare picker dialogs, native
// ScreenCaptureKit picker negotiation, screenshot frame captures, and
// background image processing for the contextual searchbox.
class ContextualSearchboxScreenshareController {
 public:
  // Delegate interface for UI components (e.g. OmniboxEverywhereUI) to present
  // the screenshot context menu, region selection overlay, and receive picker
  // lifecycle events.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual void ShowScreenshotMenu(
        const gfx::Rect& anchor_rect,
        base::WeakPtr<ContextualSearchboxScreenshareController> controller) {}

    // Invoked when the screenshare picker is opened or closed.
    virtual void OnScreensharePickerOpened() {}
    virtual void OnScreensharePickerClosed() {}

    using RegionCaptureSource = OmniboxEverywhereService::RegionCaptureSource;
    using RegionSelectedCallback =
        base::OnceCallback<void(const SkBitmap& result_bitmap)>;
    virtual void ShowRegionSelectOverlay(const SkBitmap& screenshot,
                                         const RegionCaptureSource& source,
                                         RegionSelectedCallback callback) {}
  };

  using RegionCaptureSource = Delegate::RegionCaptureSource;

  // Host interface for uploading processed screenshots to the contextual
  // search session and attaching them to the WebUI page.
  class Host {
   public:
    virtual ~Host() = default;

    using AddFileContextCallback = base::OnceCallback<void(
        base::expected<base::UnguessableToken,
                       contextual_search::ContextUploadErrorType>)>;

    virtual void UploadScreenshot(
        std::string file_name,
        std::string mime_type,
        mojo_base::BigBuffer file_bytes,
        std::optional<lens::ImageEncodingOptions> image_encoding_options,
        AddFileContextCallback callback) = 0;

    virtual void AddFileContextToPage(
        const base::UnguessableToken& token,
        searchbox::mojom::SelectedFileInfoPtr file_info) = 0;

    virtual void OnScreenshotMenuClosed() = 0;
  };

  struct ProcessedScreenshot {
    std::vector<uint8_t> png_bytes;
    std::optional<std::string> thumbnail_data_url;
  };

  using StartScreenshareCallback =
      searchbox::mojom::PageHandler::StartScreenshareCallback;
  using CaptureRegionScreenshotCallback =
      searchbox::mojom::PageHandler::CaptureRegionScreenshotCallback;

  ContextualSearchboxScreenshareController(
      content::WebContents* web_contents,
      Host* host,
      Delegate* delegate = nullptr,
      DesktopMediaPickerFactory* picker_factory = nullptr);
  ContextualSearchboxScreenshareController(
      const ContextualSearchboxScreenshareController&) = delete;
  ContextualSearchboxScreenshareController& operator=(
      const ContextualSearchboxScreenshareController&) = delete;
  ~ContextualSearchboxScreenshareController();

  void ShowScreenshotMenu(const gfx::Rect& anchor_rect);
  void StartScreenshare(bool prefer_entire_screen,
                        StartScreenshareCallback callback);
  void CaptureRegionScreenshot(CaptureRegionScreenshotCallback callback);
  void OnScreenshotMenuClosed();

  Delegate* delegate() const { return delegate_; }
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  bool is_capturing() const { return is_capturing_; }

  base::WeakPtr<ContextualSearchboxScreenshareController> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
#if !BUILDFLAG(IS_ANDROID)
  void StartScreenshareInternal(bool prefer_entire_screen,
                                bool is_region_capture,
                                StartScreenshareCallback callback);
  void FallbackToChromeDefaultPicker(bool prefer_entire_screen,
                                     bool is_region_capture,
                                     StartScreenshareCallback callback);
  void OnChromeDefaultPickerResults(StartScreenshareCallback callback,
                                    bool is_region_capture,
                                    const std::string& err,
                                    content::DesktopMediaID source);
  void OnChromeDefaultPickerDestroyed();
  void OnNativePickerCreated(content::DesktopMediaID::Id id);
  void OnNativePickerSourceSelected(content::DesktopMediaID::Type type,
                                    bool is_region_capture,
                                    StartScreenshareCallback callback,
                                    webrtc::DesktopCapturer::Source source);
  void OnNativePickerCancelled(StartScreenshareCallback callback);
  void CaptureFullDesktopRegionScreenshot(
      CaptureRegionScreenshotCallback callback);
  void CaptureAndUploadScreenshot(
      content::DesktopMediaID source,
      StartScreenshareCallback callback,
      std::optional<RegionCaptureSource> region_capture_source = std::nullopt);
  void OnScreenshotCaptured(
      StartScreenshareCallback callback,
      std::optional<RegionCaptureSource> region_capture_source,
      const SkBitmap& bitmap);
  void OnRegionSelected(StartScreenshareCallback callback,
                        const SkBitmap& region_bitmap);
  void OnScreenshotProcessed(StartScreenshareCallback callback,
                             ProcessedScreenshot result);
  void NotifyScreensharePickerOpened();
  void NotifyScreensharePickerClosed();
  bool IsScreenshareInProgress() const;
#endif

  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<Host> host_;
  raw_ptr<Delegate> delegate_ = nullptr;

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<DesktopMediaPickerController> screenshare_picker_controller_;
  std::unique_ptr<content::desktop_capture::ScreenshotCaptureRequest>
      active_screenshot_request_;
  raw_ptr<DesktopMediaPickerFactory> picker_factory_ = nullptr;
  bool is_native_picker_open_ = false;
  bool is_region_overlay_open_ = false;

  // State for coordinating screenshot capture with picker dialog destruction
  // when using DesktopMediaPickerController. To avoid capturing the picker
  // dialog in the screenshot while it is still closing, screenshot capture is
  // deferred until the dialog's Views widget is destroyed.
  //
  // `pending_screenshare_source_` holds the selected capture source and
  // `pending_screenshare_callback_` holds the Mojo completion callback if the
  // user selected a source before the dialog finished destroying.
  // `chrome_default_picker_destroyed_` tracks whether the dialog widget was
  // destroyed before `OnChromeDefaultPickerResults()` was invoked.
  std::optional<content::DesktopMediaID> pending_screenshare_source_;
  StartScreenshareCallback pending_screenshare_callback_;
  std::optional<RegionCaptureSource> pending_region_capture_source_;
  bool chrome_default_picker_destroyed_ = false;
#endif

  bool is_capturing_ = false;

  base::WeakPtrFactory<ContextualSearchboxScreenshareController>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_CONTEXTUAL_SEARCHBOX_SCREENSHARE_CONTROLLER_H_
