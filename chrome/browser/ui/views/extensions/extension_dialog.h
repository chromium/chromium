// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_DIALOG_H_

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/window/dialog_delegate.h"

class ExtensionDialogObserver;
class ExtensionViewViews;
class GURL;
class Profile;

namespace content {
class WebContents;
}

namespace extensions {
class ExtensionViewHost;
}

// Modal dialog containing contents provided by an extension.
// Dialog is automatically centered in the owning window and has fixed size.
// For example, used by the Chrome OS file browser.
class ExtensionDialog : public views::DialogDelegate,
                        public content::NotificationObserver,
                        public base::RefCounted<ExtensionDialog> {
 public:
  struct InitParams {
    // |size| Size in DIP (Device Independent Pixel) for the dialog window.
    explicit InitParams(gfx::Size size);
    InitParams(const InitParams& other);
    ~InitParams();

    // |is_modal| determines whether the dialog is modal to |parent_window|.
    bool is_modal = false;

    // Size in DIP (Device Independent Pixel) for the dialog window.
    gfx::Size size;

    // Minimum size in DIP (Device Independent Pixel) for the dialog window.
    gfx::Size min_size;

    // Text for the dialog title, it should be already localized.
    base::string16 title;

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // |title_color| customizes the color of the window title.
    base::Optional<SkColor> title_color;
    // |title_inactive_color| customizes the color of the window title when
    // window is inactive.
    base::Optional<SkColor> title_inactive_color;
#endif
  };
  // Create and show a dialog with |url| centered over the provided window.
  // |parent_window| is the parent window to which the pop-up will be attached.
  // |profile| is the profile that the extension is registered with.
  // |web_contents| is the tab that spawned the dialog.
  static ExtensionDialog* Show(const GURL& url,
                               gfx::NativeWindow parent_window,
                               Profile* profile,
                               content::WebContents* web_contents,
                               ExtensionDialogObserver* observer,
                               const InitParams& init_params);

  // Notifies the dialog that the observer has been destroyed and should not
  // be sent notifications.
  void ObserverDestroyed();

  // Focus to the render view if possible.
  void MaybeFocusRenderView();

  // Sets minimum contents size in pixels and makes the window resizable.
  void SetMinimumContentsSize(int width, int height);

  extensions::ExtensionViewHost* host() const { return host_.get(); }

  // views::DialogDelegate:
  ui::ModalType GetModalType() const override;
  void WindowClosing() override;
  void DeleteDelegate() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  views::View* GetContentsView() override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 protected:
  ~ExtensionDialog() override;

 private:
  friend class base::RefCounted<ExtensionDialog>;

  // Use Show() to create instances.
  ExtensionDialog(std::unique_ptr<extensions::ExtensionViewHost> host,
                  ExtensionDialogObserver* observer,
                  gfx::NativeWindow parent_window,
                  const InitParams& init_params);

  // Window Title
  base::string16 window_title_;

  // The contained host for the view.
  std::unique_ptr<extensions::ExtensionViewHost> host_;

  ExtensionViewViews* extension_view_ = nullptr;

  content::NotificationRegistrar registrar_;

  // The observer of this popup.
  ExtensionDialogObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_DIALOG_H_
