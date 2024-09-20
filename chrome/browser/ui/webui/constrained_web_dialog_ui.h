// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONSTRAINED_WEB_DIALOG_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CONSTRAINED_WEB_DIALOG_UI_H_

#include <memory>

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Size;
}

namespace content {
class BrowserContext;
class WebContents;
}

namespace ui {
class WebDialogDelegate;
}

class ConstrainedWebDialogUI;

class ConstrainedWebDialogUIConfig
    : public content::DefaultWebUIConfig<ConstrainedWebDialogUI> {
 public:
  ConstrainedWebDialogUIConfig();
};

class ConstrainedWebDialogDelegate {
 public:
  virtual const ui::WebDialogDelegate* GetWebDialogDelegate() const = 0;
  virtual ui::WebDialogDelegate* GetWebDialogDelegate() = 0;

  // Called when the dialog is being closed in response to a "dialogClose"
  // message from WebUI.
  virtual void OnDialogCloseFromWebUI() = 0;

  // If called, the dialog will release the ownership of its WebContents.
  // The dialog will continue to use it until it is destroyed.
  virtual std::unique_ptr<content::WebContents> ReleaseWebContents() = 0;

  // Returns the WebContents owned by the constrained window.
  virtual content::WebContents* GetWebContents() = 0;

  // Returns the native type used to display the dialog.
  virtual gfx::NativeWindow GetNativeDialog() = 0;

  // Returns the minimum size for the dialog.
  virtual gfx::Size GetConstrainedWebDialogMinimumSize() const = 0;

  // Returns the maximum size for the dialog.
  virtual gfx::Size GetConstrainedWebDialogMaximumSize() const = 0;

  // Returns the preferred size for the dialog, or an empty size if
  // the dialog has been closed.
  virtual gfx::Size GetConstrainedWebDialogPreferredSize() const = 0;

 protected:
  virtual ~ConstrainedWebDialogDelegate() {}
};

// ConstrainedWebDialogUI is a facility to show HTML WebUI content
// in a tab-modal constrained dialog.  It is implemented as an adapter
// between an WebDialogUI object and a web contents modal dialog.
//
// Since the web contents modal dialog requires platform-specific delegate
// implementations, this class is just a factory stub.
class ConstrainedWebDialogUI : public content::WebUIController {
 public:
  explicit ConstrainedWebDialogUI(content::WebUI* web_ui);
  ~ConstrainedWebDialogUI() override;
  ConstrainedWebDialogUI(const ConstrainedWebDialogUI&) = delete;
  ConstrainedWebDialogUI& operator=(const ConstrainedWebDialogUI&) = delete;

  // WebUIController implementation:
  void WebUIRenderFrameCreated(
      content::RenderFrameHost* render_frame_host) override;

  // Sets the delegate on the WebContents.
  static void SetConstrainedDelegate(content::WebContents* web_contents,
                                     ConstrainedWebDialogDelegate* delegate);
  static void ClearConstrainedDelegate(content::WebContents* web_contents);

 protected:
  // Returns the ConstrainedWebDialogDelegate saved with the WebContents.
  // Returns NULL if no such delegate is set.
  ConstrainedWebDialogDelegate* GetConstrainedDelegate();

 private:
  // JS Message Handler
  void OnDialogCloseMessage(const base::Value::List& args);
};

// Create and show a constrained HTML dialog. The actual object that gets
// created is a ConstrainedWebDialogDelegate, which later triggers construction
// of a ConstrainedWebDialogUI object.
// |browser_context| is used to construct the constrained HTML dialog's
//                   WebContents.
// |delegate| controls the behavior of the dialog.
// |overshadowed| is the tab being overshadowed by the dialog.
ConstrainedWebDialogDelegate* ShowConstrainedWebDialog(
    content::BrowserContext* browser_context,
    std::unique_ptr<ui::WebDialogDelegate> delegate,
    content::WebContents* overshadowed);

// Create and show a constrained HTML dialog with auto-resize enabled. The
// dialog is shown automatically after document load has completed to avoid UI
// jankiness.
// |browser_context| is used to construct the dialog's WebContents.
// |delegate| controls the behavior of the dialog.
// |overshadowed| is the tab being overshadowed by the dialog.
// |min_size| is the minimum size of the dialog.
// |max_size| is the maximum size of the dialog.
ConstrainedWebDialogDelegate* ShowConstrainedWebDialogWithAutoResize(
    content::BrowserContext* browser_context,
    std::unique_ptr<ui::WebDialogDelegate> delegate,
    content::WebContents* overshadowed,
    const gfx::Size& min_size,
    const gfx::Size& max_size);

#endif  // CHROME_BROWSER_UI_WEBUI_CONSTRAINED_WEB_DIALOG_UI_H_
