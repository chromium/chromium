// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_DIALOGS_CHROME_WEBUI_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_DIALOGS_CHROME_WEBUI_DIALOG_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Widget;
class WebView;
class View;
class Widget;
}  // namespace views

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace webui_dialog {

// Defines the configuration for a TopChrome WebUI Dialog.
struct WebDialogSpec {
  WebDialogSpec();
  ~WebDialogSpec();
  WebDialogSpec(const WebDialogSpec&);

  // The minimum and maximum size of the dialog.
  // The dialog will auto-resize within these bounds based on the WebUI content.
  // To fix a dimension, set min == max.
  gfx::Size min_size;
  gfx::Size max_size;

  // If set, overrides the default corner radius used for clipping the WebUI
  // content. If not set, defaults to
  // `views::DialogDelegate::GetCornerRadius()`.
  std::optional<int> corner_radius;

  // If true, the dialog will remain hidden until the WebUI explicitly requests
  // to be shown (via `ShowUI()`). This prevents flicker while the WebUI is
  // loading or rendering its initial state.
  bool wait_for_explicit_show = true;

  // The modality of the dialog.
  // - kNone (Default): A modeless, unanchored dialog.
  // - kWindow: A browser-modal dialog.
  // - kChild: A tab-modal dialog (requires `parent_web_contents` to be set).
  ui::mojom::ModalType modal_type = ui::mojom::ModalType::kNone;

  // Whether to show the native OS close button.
  bool show_close_button = false;

  // Optional parent tab for displaying as a tab-modal (kChild) dialog.
  base::WeakPtr<tabs::TabInterface> parent_tab;

  // Optional element ID used for the dialog.
  ui::ElementIdentifier element_identifier;
};

// A reusable dialog delegate that hosts a TopChrome WebUI page.
// This class handles the boilerplate for creating a dialog that contains a
// `WebView`, manages its lifecycle, and handles auto-resizing based on the
// WebUI content.
//
// It implements `WebUIContentsWrapper::Host` to receive signals from the
// WebContents, such as when to show or close the UI, and when the content
// size changes.
class ChromeWebUIDialog : public views::DialogDelegate,
                          public WebUIContentsWrapper::Host,
                          public views::ViewObserver,
                          public views::WidgetObserver {
 public:
  ChromeWebUIDialog(std::unique_ptr<WebUIContentsWrapper> contents_wrapper,
                    const WebDialogSpec& spec);

  ChromeWebUIDialog(const ChromeWebUIDialog&) = delete;
  ChromeWebUIDialog& operator=(const ChromeWebUIDialog&) = delete;

  ~ChromeWebUIDialog() override;

  // Creates and shows a dialog with the given `contents_wrapper` and `spec`.
  // Returns the created widget. By default, the Widget returned is
  // CLIENT_OWNS_WIDGET. Therefore, the caller is responsible to manage the
  // lifetime. Additionally, caller can use `Widget::MakeCloseSynchronous()` to
  // intercept close events from the created widget.
  static std::unique_ptr<views::Widget> Show(
      gfx::NativeWindow parent,
      std::unique_ptr<WebUIContentsWrapper> contents_wrapper,
      const WebDialogSpec& spec);

  // WebUIContentsWrapper::Host:
  void ShowUI() override;
  void CloseUI() override;
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;

  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override;

  // views::ViewObserver:
  void OnViewAddedToWidget(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

  views::WebView* web_view() { return web_view_; }

 private:
  const WebDialogSpec spec_;
  std::unique_ptr<WebUIContentsWrapper> contents_wrapper_;

  // The WebView that hosts the WebUI content.
  raw_ptr<views::WebView> web_view_ = nullptr;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  base::ScopedObservation<views::View, views::ViewObserver> view_observation_{
      this};

  base::WeakPtrFactory<ChromeWebUIDialog> weak_ptr_factory_{this};
};

}  // namespace webui_dialog

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_DIALOGS_CHROME_WEBUI_DIALOG_H_
