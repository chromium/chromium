// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CLIPBOARD_CLIPBOARD_IMAGE_MODEL_REQUEST_H_
#define CHROME_BROWSER_UI_ASH_CLIPBOARD_CLIPBOARD_IMAGE_MODEL_REQUEST_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/models/image_model.h"

namespace ash {
class ScopedClipboardHistoryPause;
}  // namespace ash

namespace ui {
class ClipboardData;
}  // namespace ui

namespace views {
class WebView;
class Widget;
}  // namespace views

class Profile;

// Renders html in an off-screen WebView, copies the rendered surface, and
// passes the copy through |deliver_image_model_callback_|. If the request takes
// takes more than 5s to load, timeout is declared and the callback is not
// called. If the request is Stop()-ed, the callback is not called.
class ClipboardImageModelRequest : public content::WebContentsDelegate,
                                   public content::WebContentsObserver {
 public:
  using ImageModelCallback = base::OnceCallback<void(ui::ImageModel)>;

  struct Params {
    Params() = delete;
    Params(const base::UnguessableToken& id,
           const std::string& html_markup,
           const gfx::Size& bounding_box_size,
           ImageModelCallback request_finished_callback);
    Params(Params&&);
    Params& operator=(Params&&);
    ~Params();

    // A unique identifier, used to cancel running requests.
    base::UnguessableToken id;
    // Markup being rendered.
    std::string html_markup;

    // The size of the rectangle which encloses the selection region on the
    // original page. It is used to calculate the size of the HTML preview image
    // shown on the clipboard history menu.
    gfx::Size bounding_box_size;

    // The callback to return the results of the request. Not called if the
    // request is stopped via Stop(), or if timeout occurs.
    ImageModelCallback callback;
  };

  using RequestStopCallback = base::RepeatingCallback<void(bool)>;

  struct TestParams {
    TestParams() = delete;
    explicit TestParams(
        RequestStopCallback callback,
        const std::optional<bool>& enforce_auto_resize = std::nullopt);
    TestParams(const TestParams&) = delete;
    TestParams& operator=(const TestParams&) = delete;
    ~TestParams();

    // The callback running when the image model request stops. The boolean
    // value indicates whether the image model request renders web contents
    // in auto resize mode.
    RequestStopCallback callback;

    // When set, `enforce_auto_resize` specifies whether the image model request
    // should be rendered in auto resize mode. If `enforce_auto_resize` is
    // true (or false), auto resize mode is always enabled (or disabled).
    std::optional<bool> enforce_auto_resize;
  };

  // Places `html_markup` on the clipboard and restores the original clipboard
  // contents when destructed.
  class ScopedClipboardModifier {
   public:
    explicit ScopedClipboardModifier(const std::string& html_markup);
    ScopedClipboardModifier(const ScopedClipboardModifier&) = delete;
    ScopedClipboardModifier& operator=(const ScopedClipboardModifier&) = delete;
    ~ScopedClipboardModifier();

   private:
    // Pauses ash::ClipboardHistory for its lifetime.
    std::unique_ptr<ash::ScopedClipboardHistoryPause>
        scoped_clipboard_history_pause_;
    std::unique_ptr<ui::ClipboardData> replaced_clipboard_data_;
  };

  ClipboardImageModelRequest(
      Profile* profile,
      base::RepeatingClosure on_request_finished_callback);
  ClipboardImageModelRequest(const ClipboardImageModelRequest&) = delete;
  ClipboardImageModelRequest operator=(const ClipboardImageModelRequest&) =
      delete;
  ~ClipboardImageModelRequest() override;

  // Renders the HTML in a WebView and attempts to copy the surface. If this
  // fails to load after 5 seconds, OnTimeout is called.
  void Start(Params&& params);

  // The different reasons a request can `Stop()`. These values are logged to
  // UMA. Entries should not be renumbered and numeric values should never be
  // reused. Please keep in sync with "RequestStopReason" in
  // src/tools/metrics/histograms/enums.xml.
  enum class RequestStopReason {
    kFulfilled = 0,
    kTimeout = 1,
    kEmptyResult = 2,
    kMultipleCopyCompletion = 3,
    kRequestCanceled = 4,
    kMaxValue = kRequestCanceled,
  };
  // Stops the request and resets state. `stop_reason` is the reason the request
  // was `Stop()`-ed. |web_view_| is kept alive to enable fast restarting of the
  // request.
  void Stop(RequestStopReason stop_reason);

  // `Stop()`s the request and gets the params of the running request.
  Params StopAndGetParams();

  // Whether the clipboard is being modified by this request.
  bool IsModifyingClipboard() const;

  // Returns whether a request with |request_id| is running, or if any request
  // is running if no |request_id| is supplied.
  bool IsRunningRequest(
      std::optional<base::UnguessableToken> request_id = std::nullopt) const;

  // content::WebContentsDelegate:
  void ResizeDueToAutoResize(content::WebContents* web_contents,
                             const gfx::Size& new_size) override;

  // content::WebContentsObserver:
  void DidStopLoading() override;

  // Configures test parameter.
  static void SetTestParams(TestParams* test_params);

 private:
  // Called when the results of the paste are painted.
  void OnVisualStateChangeFinished(bool done);

  // Posts `CopySurface()` to the task sequence.
  void PostCopySurfaceTask();

  // Copies the rendered HTML.
  void CopySurface();

  // Callback called when the rendered surface is done being copied.
  void OnCopyComplete(float device_scale_factor, const SkBitmap& bitmap);

  // Called when the running request takes too long to complete.
  void OnTimeout();

  // Returns whether the auto-resize mode should be enabled. If auto-resize mode
  // is enabled, Blink decides the preview image's size which may be different
  // from that of the original selection region.
  bool ShouldEnableAutoResizeMode() const;

  // A Widget that is not shown, but forces |web_view_| to render.
  std::unique_ptr<views::Widget> const widget_;

  // Contents view of |widget_|. Owned by |widget_|.
  const raw_ptr<views::WebView> web_view_;

  // Unique identifier for this request run. Empty when there are no running
  // requests.
  base::UnguessableToken request_id_;

  // The HTML being rendered.
  std::string html_markup_;

  // The size of the rectangle enclosing the copied HTML on the original page.
  gfx::Size bounding_box_size_;

  // Whether `DidStopLoading()` was called. Used to prevent the request from
  // responding to load events that happen after the initial load.
  bool did_stop_loading_ = false;

  // Responsible for temporarily replacing contents of the clipboard.
  std::optional<ScopedClipboardModifier> scoped_clipboard_modifier_;

  // Callback used to deliver the rendered ImageModel.
  ImageModelCallback deliver_image_model_callback_;

  // Callback called when this request finishes (via timeout or completion).
  base::RepeatingClosure on_request_finished_callback_;

  // Timer used to abort requests which take longer than 5s to load.
  base::RepeatingTimer timeout_timer_;

  // Time this object was created. Used to log object lifetime.
  const base::TimeTicks request_creation_time_;

  // Time this object started its most recent request.
  base::TimeTicks request_start_time_;

  base::WeakPtrFactory<ClipboardImageModelRequest> weak_ptr_factory_{this};

  // Used to debounce calls to `CopySurface()`.
  base::WeakPtrFactory<ClipboardImageModelRequest>
      copy_surface_weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_CLIPBOARD_CLIPBOARD_IMAGE_MODEL_REQUEST_H_
