// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_enable_flow.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/supervised_user/supervised_user_browser_utils.h"
#include "chrome/browser/ui/extensions/extension_enable_flow_delegate.h"
#include "extensions/browser/api/management/management_api.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/profiles/profile_picker.h"
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

using extensions::Extension;

ExtensionEnableFlow::ExtensionEnableFlow(Profile* profile,
                                         const std::string& extension_id,
                                         ExtensionEnableFlowDelegate* delegate)
    : profile_(profile), extension_id_(extension_id), delegate_(delegate) {}

ExtensionEnableFlow::~ExtensionEnableFlow() = default;

void ExtensionEnableFlow::StartForWebContents(
    content::WebContents* parent_contents) {
  parent_contents_ = parent_contents;
  parent_window_ = nullptr;
  Run();
}

void ExtensionEnableFlow::StartForNativeWindow(
    gfx::NativeWindow parent_window) {
  parent_contents_ = nullptr;
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

    // |extension| could be nullptr for asynchronous load, such as the case of
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

  extensions::SupervisedUserExtensionsDelegate*
      supervised_user_extensions_delegate =
          extensions::ManagementAPI::GetFactoryInstance()
              ->Get(profile_)
              ->GetSupervisedUserExtensionsDelegate();
  DCHECK(supervised_user_extensions_delegate);
  if (supervised_user::AreExtensionsPermissionsEnabled(profile_) && extension &&

      // Only ask for parent approval if the extension still requires approval.
      !supervised_user_extensions_delegate->IsExtensionAllowedByParent(
          *extension)) {
    // Either ask for parent permission or notify the child that their parent
    // has disabled this action.
    auto extension_approval_callback =
        base::BindOnce(&ExtensionEnableFlow::OnExtensionApprovalDone,
                       weak_ptr_factory_.GetWeakPtr());
    supervised_user_extensions_delegate->RequestToEnableExtensionOrShowError(
        *extension, parent_contents_,
        SupervisedUserExtensionParentApprovalEntryPoint::
            kOnTerminatedExtensionEnableFlowOperation,
        std::move(extension_approval_callback));
    return;
  }

  bool abort = !extension ||
               // The extension might be force-disabled by policy.
               system->management_policy()->MustRemainDisabled(
                   extension, nullptr, nullptr);
  if (abort) {
    delegate_->ExtensionEnableFlowAborted(
        /*user_initiated=*/false);  // |delegate_| may delete us.
    return;
  }

  if (profiles::IsProfileLocked(profile_->GetPath())) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
        ProfilePicker::EntryPoint::kProfileLocked));

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
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
  prompt_->ShowDialog(base::BindOnce(&ExtensionEnableFlow::InstallPromptDone,
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

void ExtensionEnableFlow::OnExtensionApprovalDone(
    extensions::SupervisedUserExtensionsDelegate::ExtensionApprovalResult
        result) {
  switch (result) {
    case extensions::SupervisedUserExtensionsDelegate::ExtensionApprovalResult::
        kApproved:
      EnableExtension();
      break;
    case extensions::SupervisedUserExtensionsDelegate::ExtensionApprovalResult::
        kCanceled:
      delegate_->ExtensionEnableFlowAborted(
          /*user_initiated=*/true);  // |delegate_| may delete us.
      break;
    case extensions::SupervisedUserExtensionsDelegate::ExtensionApprovalResult::
        kFailed:
    case extensions::SupervisedUserExtensionsDelegate::ExtensionApprovalResult::
        kBlocked:
      delegate_->ExtensionEnableFlowAborted(
          /*user_initiated=*/false);  // |delegate_| may delete us.
      break;
  }
}

void ExtensionEnableFlow::StartObserving() {
  extension_registry_observation_.Observe(
      extensions::ExtensionRegistry::Get(profile_));
  load_error_observation_.Observe(extensions::LoadErrorReporter::GetInstance());
}

void ExtensionEnableFlow::StopObserving() {
  extension_registry_observation_.Reset();
  load_error_observation_.Reset();
}

void ExtensionEnableFlow::OnLoadFailure(
    content::BrowserContext* browser_context,
    const base::FilePath& file_path,
    const std::string& error) {
  StopObserving();
  delegate_->ExtensionEnableFlowAborted(
      /*user_initiated=*/false);  // |delegate_| may delete us.
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
    delegate_->ExtensionEnableFlowAborted(
        /*user_initiated=*/false);  // |delegate_| may delete us.
  }
}

void ExtensionEnableFlow::EnableExtension() {
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile_);
  // The extension can be uninstalled in another window while the UI was
  // showing. Treat it as a cancellation and notify |delegate_|.
  const Extension* extension =
      registry->disabled_extensions().GetByID(extension_id_);
  if (!extension) {
    delegate_->ExtensionEnableFlowAborted(
        /*user_initiated=*/true);  // |delegate_| may delete us.
    return;
  }
  if (supervised_user::AreExtensionsPermissionsEnabled(profile_)) {
    // We need to add parent approval first.
    extensions::SupervisedUserExtensionsDelegate*
        supervised_user_extensions_delegate =
            extensions::ManagementAPI::GetFactoryInstance()
                ->Get(profile_)
                ->GetSupervisedUserExtensionsDelegate();
    CHECK(supervised_user_extensions_delegate);
    supervised_user_extensions_delegate->AddExtensionApproval(*extension);
    supervised_user_extensions_delegate->MaybeRecordPermissionsIncreaseMetrics(*extension);
    supervised_user_extensions_delegate->RecordExtensionEnablementUmaMetrics(
        /*enabled=*/true);
  }
  service->GrantPermissionsAndEnableExtension(extension);

  DCHECK(service->IsExtensionEnabled(extension_id_));
  delegate_->ExtensionEnableFlowFinished();  // |delegate_| may delete us.
}

void ExtensionEnableFlow::InstallPromptDone(
    ExtensionInstallPrompt::DoneCallbackPayload payload) {
  switch (payload.result) {
    case ExtensionInstallPrompt::Result::ACCEPTED:
      EnableExtension();
      break;
    case ExtensionInstallPrompt::Result::ACCEPTED_WITH_WITHHELD_PERMISSIONS:
      // This dialog doesn't support the "withhold permissions" checkbox.
      NOTREACHED_IN_MIGRATION();
      break;
    case ExtensionInstallPrompt::Result::USER_CANCELED:
    case ExtensionInstallPrompt::Result::ABORTED:
      delegate_->ExtensionEnableFlowAborted(/*user_initiated=*/
                                            payload.result ==
                                            ExtensionInstallPrompt::Result::
                                                USER_CANCELED);
      // `delegate_` may delete us.
      break;
  }
}
