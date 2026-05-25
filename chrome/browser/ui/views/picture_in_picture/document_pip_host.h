// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_HOST_H_

#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/mojom/picture_in_picture_window_options/picture_in_picture_window_options.mojom.h"
#include "ui/views/widget/widget.h"

namespace views {
class Widget;
class WidgetDelegate;
}  // namespace views

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
class DocumentPipHost : public content::WebContentsUserData<DocumentPipHost>,
                        public content::WebContentsObserver,
                        public content::WebContentsDelegate {
 public:
  DocumentPipHost(const DocumentPipHost&) = delete;
  DocumentPipHost& operator=(const DocumentPipHost&) = delete;

  ~DocumentPipHost() override;

  // Creates the PiP widget for the child WebContents. Must be called after
  // construction (i.e. after CreateForWebContents). The host can exist as
  // UserData without a widget; call this to actually open the PiP window.
  void CreatePipWidget();

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

 private:
  friend class content::WebContentsUserData<DocumentPipHost>;

  // Private constructor called by WebContentsUserData machinery via
  // CreateForWebContents().
  DocumentPipHost(content::WebContents* opener_web_contents,
                  std::unique_ptr<content::WebContents> child_web_contents,
                  blink::mojom::PictureInPictureWindowOptions pip_options);

  // Tears down the PiP widget and child WebContents. Safe to call multiple
  // times; subsequent calls are no-ops. Uses widget_.reset() /
  // child_web_contents_.reset() for synchronous CLIENT_OWNS_WIDGET teardown.
  void ClosePipWindow();

  // Callback for Widget::MakeCloseSynchronous(). Invoked when external code
  // (e.g. DialogDelegate, OS close button) requests the widget to close.
  void OnWidgetCloseRequested(views::Widget::ClosedReason reason);

  // The child WebContents rendered inside the PiP window.
  std::unique_ptr<content::WebContents> child_web_contents_;

  // Placeholder delegate for the floating Widget. Must be declared before
  // |widget_| so it is destroyed after |widget_|.
  std::unique_ptr<views::WidgetDelegate> widget_delegate_;

  // The floating window hosting |child_web_contents_|.
  std::unique_ptr<views::Widget> widget_;

  // Initial options from the requestWindow() call.
  blink::mojom::PictureInPictureWindowOptions pip_options_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_PICTURE_IN_PICTURE_DOCUMENT_PIP_HOST_H_
