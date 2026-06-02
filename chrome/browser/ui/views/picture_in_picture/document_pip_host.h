// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_HOST_H_

#include "chrome/browser/picture_in_picture/picture_in_picture_window.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/mojom/picture_in_picture_window_options/picture_in_picture_window_options.mojom.h"
#include "ui/views/widget/widget.h"

class DocumentPipWidgetDelegate;
class PictureInPictureTucker;
class Profile;

namespace content {
class WebContents;
}  // namespace content

// DocumentPipHost is the standalone Document Picture-in-Picture host.
// It is attached to the *opener* WebContents as a WebContentsUserData and:
//   - Owns the child WebContents for the PiP window.
//   - Owns the floating views::Widget that renders that child.
//   - Acts as WebContentsDelegate for the child WebContents.
//   - Observes the opener WebContents to close the PiP window when the opener
//     is destroyed or navigates to a new primary page.
//   - Implements PictureInPictureWindow for tucking and Mac fullscreen.
class DocumentPipHost : public content::WebContentsUserData<DocumentPipHost>,
                        public content::WebContentsObserver,
                        public content::WebContentsDelegate,
                        public PictureInPictureWindow {
 public:
  DocumentPipHost(const DocumentPipHost&) = delete;
  DocumentPipHost& operator=(const DocumentPipHost&) = delete;

  ~DocumentPipHost() override;

  // Creates the PiP widget for the given child WebContents. Can be called
  // multiple times over the host's lifetime — each call opens a new PiP window
  // after the previous one has been closed via ClosePipWindow(). The child
  // WebContents ownership is transferred to the widget's WebView.
  void CreatePipWidget(std::unique_ptr<content::WebContents> child_web_contents,
                       blink::mojom::PictureInPictureWindowOptions pip_options);

  // Accessors.
  Profile* GetProfile();
  content::WebContents* GetOpenerWebContents();
  content::WebContents* GetChildWebContents();
  views::Widget* GetWidget();
  const blink::mojom::PictureInPictureWindowOptions& GetPipOptions() const;

  // content::WebContentsObserver (observing the opener):
  void PrimaryPageChanged(content::Page& page) override;

  // content::WebContentsDelegate (serving the child):
  blink::mojom::DisplayMode GetDisplayMode(
      const content::WebContents* web_contents) override;
  void CloseContents(content::WebContents* source) override;

  // PictureInPictureWindow:
  void SetForcedTucking(bool tuck) override;
#if BUILDFLAG(IS_MAC)
  void OnAnyBrowserEnteredFullscreen() override;
#endif

 private:
  friend class content::WebContentsUserData<DocumentPipHost>;

  // Private constructor called by WebContentsUserData machinery via
  // CreateForWebContents().
  explicit DocumentPipHost(content::WebContents* opener_web_contents);

  // Tears down the PiP widget (and with it the child WebContents, which is
  // owned by the WebView inside the widget's contents view). Safe to call
  // multiple times; subsequent calls are no-ops.
  void ClosePipWindow();

  // Callback for Widget::MakeCloseSynchronous(). Invoked when external code
  // (e.g. DialogDelegate, OS close button) requests the widget to close.
  void OnWidgetCloseRequested(views::Widget::ClosedReason reason);

  // The delegate for the floating Widget. Owned by this host (not by the
  // Widget): `CLIENT_OWNS_WIDGET` + not `SetOwnedByWidget()` means the Widget
  // never deletes it, so the client must. Declared before `widget_` so it is
  // destroyed after the Widget, since the Widget references it via a raw
  // pointer set in Init().
  std::unique_ptr<DocumentPipWidgetDelegate> widget_delegate_;

  // The floating window hosting the PiP child WebContents.
  // Declared after `widget_delegate_` so it is destroyed first: members are
  // destroyed in reverse declaration order.
  std::unique_ptr<views::Widget> widget_;

  // Initial options from the requestWindow() call.
  blink::mojom::PictureInPictureWindowOptions pip_options_;

  // Manages tucking the PiP window offscreen. Created lazily on first
  // SetForcedTucking() call.
  std::unique_ptr<PictureInPictureTucker> tucker_;
  bool is_tucking_forced_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_HOST_H_
