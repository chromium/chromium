// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ENABLE_FLOW_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ENABLE_FLOW_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"
#include "extensions/common/extension_id.h"

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
class ExtensionEnableFlow : public extensions::LoadErrorReporter::Observer,
                            public extensions::ExtensionRegistryObserver {
 public:
  ExtensionEnableFlow(Profile* profile,
                      const std::string& extension_id,
                      ExtensionEnableFlowDelegate* delegate);

  ExtensionEnableFlow(const ExtensionEnableFlow&) = delete;
  ExtensionEnableFlow& operator=(const ExtensionEnableFlow&) = delete;

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

  const extensions::ExtensionId& extension_id() const { return extension_id_; }

  // LoadErrorReporter::Observer:
  void OnLoadFailure(content::BrowserContext* browser_context,
                     const base::FilePath& file_path,
                     const std::string& error) override;

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

  // Called when the extension approval flow is complete.
  void OnExtensionApprovalDone(
      extensions::SupervisedUserExtensionsDelegate::ExtensionApprovalResult
          result);

  // Starts/stops observing extension load notifications.
  void StartObserving();
  void StopObserving();

  // extensions::ExtensionRegistryObserver overrides:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  void EnableExtension();

  void InstallPromptDone(ExtensionInstallPrompt::DoneCallbackPayload payload);

  const raw_ptr<Profile> profile_;
  const extensions::ExtensionId extension_id_;
  const raw_ptr<ExtensionEnableFlowDelegate> delegate_;  // Not owned.

  // Parent web contents for ExtensionInstallPrompt that may be created during
  // the flow. Note this is mutually exclusive with |parent_window_| below.
  raw_ptr<content::WebContents> parent_contents_ = nullptr;

  // Parent native window for ExtensionInstallPrompt. Note this is mutually
  // exclusive with |parent_contents_| above.
  gfx::NativeWindow parent_window_ = gfx::NativeWindow();

  std::unique_ptr<ExtensionInstallPrompt> prompt_;

  // Listen to extension load notification.
  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observation_{this};

  base::ScopedObservation<extensions::LoadErrorReporter,
                          extensions::LoadErrorReporter::Observer>
      load_error_observation_{this};

  base::WeakPtrFactory<ExtensionEnableFlow> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_ENABLE_FLOW_H_
