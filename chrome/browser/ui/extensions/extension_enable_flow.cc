// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_enable_flow.h"

#include <memory>

#include "base/bind.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/user_manager.h"
#endif  // !defined(OS_CHROMEOS)

using extensions::Extension;

ExtensionEnableFlow::ExtensionEnableFlow(Profile* profile,
                                         const std::string& extension_id,
                                         ExtensionEnableFlowDelegate* delegate)
    : profile_(profile), extension_id_(extension_id), delegate_(delegate) {}

ExtensionEnableFlow::~ExtensionEnableFlow() = default;

void ExtensionEnableFlow::StartForWebContents(
    content::WebContents* parent_contents) {
  parent_contents_ = parent_contents;
  parent_window_ = NULL;
  Run();
}

void ExtensionEnableFlow::StartForNativeWindow(
    gfx::NativeWindow parent_window) {
  parent_contents_ = NULL;
  parent_window_ = parent_window;
  Run();
}

void ExtensionEnableFlow::Start() {
  Run();
}

void ExtensionEnableFlow::Run() {
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);
  const Extension* extension =
      registry->disabled_extensions().GetByID(extension_id_);
  if (!extension) {
    extension = registry->terminated_extensions().GetByID(extension_id_);
    // It's possible (though unlikely) the app could have been uninstalled since
    // the user clicked on it.
    if (!extension)
      return;
    // If the app was terminated, reload it first.
    service->ReloadExtension(extension_id_);

    // ReloadExtension reallocates the Extension object.
    extension = registry->disabled_extensions().GetByID(extension_id_);

    // |extension| could be NULL for asynchronous load, such as the case of
    // an unpacked extension. Wait for the load to continue the flow.
    if (!extension) {
      StartObserving();
      return;
    }
  }

  CheckPermissionAndMaybePromptUser();
}

void ExtensionEnableFlow::CheckPermissionAndMaybePromptUser() {
  extensions::ExtensionSystem* system =
      extensions::ExtensionSystem::Get(profile_);
  extensions::ExtensionService* service = system->extension_service();
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);
  const Extension* extension =
      registry->disabled_extensions().GetByID(extension_id_);

  bool abort = !extension ||
               // The extension might be force-disabled by policy.
               system->management_policy()->MustRemainDisabled(
                   extension, nullptr, nullptr);
  if (abort) {
    delegate_->ExtensionEnableFlowAborted(false);  // |delegate_| may delete us.
    return;
  }

  if (profiles::IsProfileLocked(profile_->GetPath())) {
#if !defined(OS_CHROMEOS)
    UserManager::Show(base::FilePath(),
                      profiles::USER_MANAGER_SELECT_PROFILE_NO_ACTION);
#endif  // !defined(OS_CHROMEOS)
    return;
  }

  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(profile_);
  if (!prefs->DidExtensionEscalatePermissions(extension_id_)) {
    // Enable the extension immediately if its privileges weren't escalated.
    // This is a no-op if the extension was previously terminated.
    service->EnableExtension(extension_id_);

    DCHECK(service->IsExtensionEnabled(extension_id_));
    delegate_->ExtensionEnableFlowFinished();  // |delegate_| may delete us.
    return;
  }

  CreatePrompt();
  ExtensionInstallPrompt::PromptType type =
      ExtensionInstallPrompt::GetReEnablePromptTypeForExtension(profile_,
                                                                extension);
  prompt_->ShowDialog(base::Bind(&ExtensionEnableFlow::InstallPromptDone,
                                 weak_ptr_factory_.GetWeakPtr()),
                      extension, nullptr,
                      std::make_unique<ExtensionInstallPrompt::Prompt>(type),
                      ExtensionInstallPrompt::GetDefaultShowDialogCallback());
}

void ExtensionEnableFlow::CreatePrompt() {
  prompt_.reset(parent_contents_
                    ? new ExtensionInstallPrompt(parent_contents_)
                    : new ExtensionInstallPrompt(profile_, nullptr));
}

void ExtensionEnableFlow::StartObserving() {
  extension_registry_observer_.Add(
      extensions::ExtensionRegistry::Get(profile_));
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_LOAD_ERROR,
                 content::Source<Profile>(profile_));
}

void ExtensionEnableFlow::StopObserving() {
  registrar_.RemoveAll();
  extension_registry_observer_.RemoveAll();
}

void ExtensionEnableFlow::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  DCHECK_EQ(extensions::NOTIFICATION_EXTENSION_LOAD_ERROR, type);
  StopObserving();
  delegate_->ExtensionEnableFlowAborted(false);
}

void ExtensionEnableFlow::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  if (extension->id() == extension_id_) {
    StopObserving();
    CheckPermissionAndMaybePromptUser();
  }
}

void ExtensionEnableFlow::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  if (extension->id() == extension_id_) {
    StopObserving();
    delegate_->ExtensionEnableFlowAborted(false);
  }
}

void ExtensionEnableFlow::InstallPromptDone(
    ExtensionInstallPrompt::Result result) {
  if (result == ExtensionInstallPrompt::Result::ACCEPTED) {
    extensions::ExtensionService* service =
        extensions::ExtensionSystem::Get(profile_)->extension_service();
    extensions::ExtensionRegistry* registry =
        extensions::ExtensionRegistry::Get(profile_);
    // The extension can be uninstalled in another window while the UI was
    // showing. Treat it as a cancellation and notify |delegate_|.
    const Extension* extension =
        registry->disabled_extensions().GetByID(extension_id_);
    if (!extension) {
      delegate_->ExtensionEnableFlowAborted(true);
      return;
    }

    service->GrantPermissionsAndEnableExtension(extension);

    DCHECK(service->IsExtensionEnabled(extension_id_));
    delegate_->ExtensionEnableFlowFinished();  // |delegate_| may delete us.
  } else {
    delegate_->ExtensionEnableFlowAborted(
        result == ExtensionInstallPrompt::Result::USER_CANCELED);
    // |delegate_| may delete us.
  }
}
