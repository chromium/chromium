// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_DIALOG_H_

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
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
  // Create and show a dialog with |url| centered over the provided window.
  // |parent_window| is the parent window to which the pop-up will be attached.
  // |profile| is the profile that the extension is registered with.
  // |web_contents| is the tab that spawned the dialog.
  // |is_modal| determines whether the dialog is modal to |parent_window|.
  // |width| and |height| are the size of the dialog in pixels.
  static ExtensionDialog* Show(const GURL& url,
                               gfx::NativeWindow parent_window,
                               Profile* profile,
                               content::WebContents* web_contents,
                               bool is_modal,
                               int width,
                               int height,
                               int min_width,
                               int min_height,
                               const base::string16& title,
                               ExtensionDialogObserver* observer);

  // Notifies the dialog that the observer has been destroyed and should not
  // be sent notifications.
  void ObserverDestroyed();

  // Focus to the render view if possible.
  void MaybeFocusRenderView();

  // Sets the window title.
  void set_title(const base::string16& title) { window_title_ = title; }

  // Sets minimum contents size in pixels and makes the window resizable.
  void SetMinimumContentsSize(int width, int height);

  extensions::ExtensionViewHost* host() const { return host_.get(); }

  // views::DialogDelegate override.
  bool CanResize() const override;
  ui::ModalType GetModalType() const override;
  bool ShouldShowWindowTitle() const override;
  base::string16 GetWindowTitle() const override;
  void WindowClosing() override;
  void DeleteDelegate() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  views::View* GetContentsView() override;

  // content::NotificationObserver overrides.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 protected:
  ~ExtensionDialog() override;

 private:
  friend class base::RefCounted<ExtensionDialog>;

  // Use Show() to create instances.
  ExtensionDialog(std::unique_ptr<extensions::ExtensionViewHost> host,
                  ExtensionDialogObserver* observer);

  void InitWindow(gfx::NativeWindow parent_window,
                  bool is_modal,
                  int width,
                  int height,
                  int min_width,
                  int min_height);

  ExtensionViewViews* GetExtensionView() const;
  static ExtensionViewViews* GetExtensionView(
      extensions::ExtensionViewHost* host);

  // Window Title
  base::string16 window_title_;

  // The contained host for the view.
  std::unique_ptr<extensions::ExtensionViewHost> host_;

  content::NotificationRegistrar registrar_;

  // The observer of this popup.
  ExtensionDialogObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_DIALOG_H_
