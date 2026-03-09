// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_DEFAULT_BROWSER_MODAL_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_DEFAULT_BROWSER_MODAL_DIALOG_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/window/dialog_delegate.h"

class Profile;

namespace views {
class WebView;
class Widget;
}

namespace default_browser {

// The "Modal" entrypoint for the Windows Default Browser. This dialog
// displays a WebUI page to provide prompts that help users navigate OS
// settings to set Chrome as the default browser based on the interaction
// choice.
class DefaultBrowserModalDialog : public views::DialogDelegate,
                                  public views::ViewObserver {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kDefaultBrowserModalDialogId);

  // Shows the modal dialog. This is triggered by the DefaultBrowserManager when
  // the STARTUP_MODAL entrypoint is selected.
  static views::Widget* Show(Profile* profile,
                             gfx::NativeWindow parent,
                             bool use_settings_illustration,
                             bool can_pin_to_taskbar);

  DefaultBrowserModalDialog(Profile* profile,
                            bool use_settings_illustration,
                            bool can_pin_to_taskbar);
  ~DefaultBrowserModalDialog() override;

  DefaultBrowserModalDialog(const DefaultBrowserModalDialog&) = delete;
  DefaultBrowserModalDialog& operator=(const DefaultBrowserModalDialog&) =
      delete;

  void ResizeNativeViewHeight(int height);

  // views::ViewObserver:
  void OnViewAddedToWidget(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

 private:
  raw_ptr<views::WebView> web_view_ = nullptr;
  base::ScopedObservation<views::View, views::ViewObserver>
      web_view_observation_{this};
};

}  // namespace default_browser

#endif  // CHROME_BROWSER_UI_WEBUI_DEFAULT_BROWSER_DEFAULT_BROWSER_MODAL_DIALOG_DELEGATE_H_
