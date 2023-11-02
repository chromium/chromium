// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_DIALOG_H_

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/scoped_observation.h"
#include "build/chromeos_buildflags.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_observer.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/process_manager_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/color/color_id.h"
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
                        public extensions::ExtensionHostObserver,
                        public extensions::ProcessManagerObserver,
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
    std::u16string title;

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // |title_color| customizes the color of the window title.
    absl::optional<ui::ColorId> title_color;
    // |title_inactive_color| customizes the color of the window title when
    // window is inactive.
    absl::optional<ui::ColorId> title_inactive_color;
#endif
  };

  ExtensionDialog(const ExtensionDialog&) = delete;
  ExtensionDialog& operator=(const ExtensionDialog&) = delete;

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

  // Focus to the renderer if possible.
  void MaybeFocusRenderer();

  // Sets minimum contents size in pixels and makes the window resizable.
  void SetMinimumContentsSize(int width, int height);

  extensions::ExtensionViewHost* host() const { return host_.get(); }

  // extensions::ExtensionHostObserver:
  void OnExtensionHostDidStopFirstLoad(
      const extensions::ExtensionHost* host) override;

  // extensions::ProcessManagerObserver:
  void OnExtensionProcessTerminated(
      const extensions::Extension* extension) override;
  void OnProcessManagerShutdown(extensions::ProcessManager* manager) override;

 protected:
  ~ExtensionDialog() override;

 private:
  friend class base::RefCounted<ExtensionDialog>;

  // Use Show() to create instances.
  ExtensionDialog(std::unique_ptr<extensions::ExtensionViewHost> host,
                  ExtensionDialogObserver* observer,
                  gfx::NativeWindow parent_window,
                  const InitParams& init_params);

  void OnWindowClosing();

  // Handles a signal from the `host` to close.
  void HandleCloseExtensionHost(extensions::ExtensionHost* host);

  // Window Title
  std::u16string window_title_;

  // The contained host for the view.
  std::unique_ptr<extensions::ExtensionViewHost> host_;

  raw_ptr<ExtensionViewViews> extension_view_ = nullptr;

  base::ScopedObservation<extensions::ExtensionHost,
                          extensions::ExtensionHostObserver>
      extension_host_observation_{this};

  base::ScopedObservation<extensions::ProcessManager,
                          extensions::ProcessManagerObserver>
      process_manager_observation_{this};

  // The observer of this popup.
  raw_ptr<ExtensionDialogObserver> observer_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_DIALOG_H_
