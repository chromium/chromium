// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CLIPBOARD_IMAGE_MODEL_REQUEST_H_
#define CHROME_BROWSER_UI_ASH_CLIPBOARD_IMAGE_MODEL_REQUEST_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/models/image_model.h"

namespace views {
class WebView;
class Widget;
}  // namespace views

class Profile;

// Renders html in an off-screen WebView, copies the rendered surface, and
// passes the copy through |deliver_image_model_callback_|. If the request takes
// takes more than 5s to load, timeout is declared and the callback is not
// called. If the request is Stop()-ed, the callback is not called.
class ClipboardImageModelRequest : public content::WebContentsObserver {
 public:
  using ImageModelCallback = base::OnceCallback<void(ui::ImageModel)>;

  struct Params {
    Params() = delete;
    Params(const base::UnguessableToken& id,
           const std::string& html_markup,
           ImageModelCallback request_finished_callback);
    Params(Params&&);
    Params& operator=(Params&&);
    ~Params();

    // A unique identifier, used to cancel running requests.
    base::UnguessableToken id;
    // Markup being rendered.
    std::string html_markup;
    // The callback to return the results of the request. Not called if the
    // request is stopped via Stop(), or if timeout occurs.
    ImageModelCallback callback;
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

  // Stops the request and resets state. |web_view_| is still alive to
  // enable fast restarting of the request.
  void Stop();

  // Returns whether a request with |request_id| is running, or if any request
  // is running if no |request_id| is supplied.
  bool IsRunningRequest(
      base::Optional<base::UnguessableToken> request_id = base::nullopt) const;

  // content::WebContentsObserver:
  void DidStopLoading() override;

 private:
  // Callback called when the rendered surface is done being copied.
  void OnCopyComplete(const SkBitmap& bitmap);

  // Called when the running request takes too long to complete.
  void OnTimeout();

  // A Widget that is not shown, but forces |web_view_| to render.
  std::unique_ptr<views::Widget> const widget_;

  // Contents view of |widget_|. Owned by |widget_|.
  views::WebView* const web_view_;

  // Unique identifier for this request run. Empty when there are no running
  // requests.
  base::UnguessableToken request_id_;

  // Callback used to deliver the rendered ImageModel.
  ImageModelCallback deliver_image_model_callback_;

  // Callback called when this request finishes (via timeout or completion).
  base::RepeatingClosure on_request_finished_callback_;

  // Timer used to abort requests which take longer than 5s to load.
  base::RepeatingTimer timeout_timer_;

  base::WeakPtrFactory<ClipboardImageModelRequest> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_CLIPBOARD_IMAGE_MODEL_REQUEST_H_
