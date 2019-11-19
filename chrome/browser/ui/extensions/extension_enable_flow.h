// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ENABLE_FLOW_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ENABLE_FLOW_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

class ExtensionEnableFlowDelegate;

namespace content {
class WebContents;
}

// ExtensionEnableFlow performs an UI flow to enable a disabled/terminated
// extension. It calls its delegate when enabling is done or is aborted.
// Callback on the delegate might be called synchronously if there is no
// permission change while the extension is disabled/terminated (or the
// extension is enabled already). Otherwise, a re-enable install prompt is
// shown to user. The extension is enabled when user acknowledges it or the
// flow is aborted when user declines it.
class ExtensionEnableFlow : public content::NotificationObserver,
                            public extensions::ExtensionRegistryObserver {
 public:
  ExtensionEnableFlow(Profile* profile,
                      const std::string& extension_id,
                      ExtensionEnableFlowDelegate* delegate);
  ~ExtensionEnableFlow() override;

  // Starts the flow and the logic continues on |delegate_| after enabling is
  // finished or aborted. Note that |delegate_| could be called synchronously
  // before this call returns when there is no need to show UI to finish the
  // enabling flow. Three variations of the flow are supported:
  //   - one with a parent WebContents
  //   - one with a native parent window
  //   - one with no parent
  void StartForWebContents(content::WebContents* parent_contents);
  void StartForNativeWindow(gfx::NativeWindow parent_window);
  void Start();

  const std::string& extension_id() const { return extension_id_; }

 private:
  // Runs the enable flow. It starts by checking if the extension is loaded.
  // If not, it tries to reload it. If the load is asynchronous, wait for the
  // load to finish before continuing the flow. Otherwise, calls
  // CheckPermissionAndMaybePromptUser finish the flow.
  void Run();

  // Checks if there is permission escalation while the extension is
  // disabled/terminated. If no, enables the extension and notify |delegate_|
  // synchronously. Otherwise, creates an ExtensionInstallPrompt and asks user
  // to confirm.
  void CheckPermissionAndMaybePromptUser();

  // Creates an ExtensionInstallPrompt in |prompt_|.
  void CreatePrompt();

  // Starts/stops observing extension load notifications.
  void StartObserving();
  void StopObserving();

  // content::NotificationObserver overrides:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // extensions::ExtensionRegistryObserver overrides:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  void InstallPromptDone(ExtensionInstallPrompt::Result result);

  Profile* const profile_;
  const std::string extension_id_;
  ExtensionEnableFlowDelegate* const delegate_;  // Not owned.

  // Parent web contents for ExtensionInstallPrompt that may be created during
  // the flow. Note this is mutually exclusive with |parent_window_| below.
  content::WebContents* parent_contents_ = nullptr;

  // Parent native window for ExtensionInstallPrompt. Note this is mutually
  // exclusive with |parent_contents_| above.
  gfx::NativeWindow parent_window_ = nullptr;

  // Called to acquire a parent window for the prompt. This is used for clients
  // who only want to create a window if it is required.
  base::Callback<gfx::NativeWindow(void)> window_getter_;

  std::unique_ptr<ExtensionInstallPrompt> prompt_;
  content::NotificationRegistrar registrar_;

  // Listen to extension load notification.
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_{this};

  base::WeakPtrFactory<ExtensionEnableFlow> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExtensionEnableFlow);
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ENABLE_FLOW_H_
