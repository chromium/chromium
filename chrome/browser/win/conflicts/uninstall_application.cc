// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/uninstall_application.h"

#include <wrl/client.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/win/atl.h"
#include "base/win/scoped_variant.h"
#include "chrome/browser/win/automation_controller.h"
#include "chrome/browser/win/ui_automation_util.h"
#include "chrome/installer/util/shell_util.h"

namespace uninstall_application {

namespace {

bool FindSearchBoxElement(IUIAutomation* automation,
                          IUIAutomationElement* sender,
                          IUIAutomationElement** search_box) {
  // Create a condition that will include only elements with the right
  // automation id in the tree walker.
  base::win::ScopedVariant search_box_id(
      L"SystemSettings_StorageSense_AppSizesListFilter_DisplayStringValue");
  Microsoft::WRL::ComPtr<IUIAutomationCondition> condition;
  HRESULT result = automation->CreatePropertyCondition(
      UIA_AutomationIdPropertyId, search_box_id, &condition);
  if (FAILED(result))
    return false;

  Microsoft::WRL::ComPtr<IUIAutomationTreeWalker> tree_walker;
  result = automation->CreateTreeWalker(condition.Get(), &tree_walker);
  if (FAILED(result))
    return false;

  // Setup a cache request so that the element contains the needed property
  // afterwards.
  Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> cache_request;
  result = automation->CreateCacheRequest(&cache_request);
  if (FAILED(result))
    return false;
  cache_request->AddPattern(UIA_ValuePatternId);

  result = tree_walker->GetNextSiblingElementBuildCache(
      sender, cache_request.Get(), search_box);
  return SUCCEEDED(result) && *search_box;
}

// UninstallAppController ------------------------------------------------------

class UninstallAppController {
 public:
  // Launches the Apps & Features page, ensuring the |application_name| is
  // written into the search box.
  static void Launch(const base::string16& application_name);

 private:
  class AutomationControllerDelegate;

  // The unique instance of this class.
  static UninstallAppController* instance_;

  explicit UninstallAppController(const base::string16& application_name);
  ~UninstallAppController();

  void OnUninstallFinished();

  // Allows the use of the UI Automation API.
  std::unique_ptr<AutomationController> automation_controller_;

  base::WeakPtrFactory<UninstallAppController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UninstallAppController);
};

// static
UninstallAppController* UninstallAppController::instance_ = nullptr;

// static
void UninstallAppController::Launch(const base::string16& application_name) {
  // If an instance already exists, the previous controller is deleted to make
  // sure it doesn't interfere with the current call.
  delete instance_;

  // The instance handles its own lifetime.
  instance_ = new UninstallAppController(application_name);
}

UninstallAppController::UninstallAppController(
    const base::string16& application_name)
    : weak_ptr_factory_(this) {
  auto automation_controller_delegate =
      std::make_unique<AutomationControllerDelegate>(
          base::SequencedTaskRunnerHandle::Get(),
          base::BindOnce(&UninstallAppController::OnUninstallFinished,
                         weak_ptr_factory_.GetWeakPtr()),
          application_name);

  automation_controller_ = std::make_unique<AutomationController>(
      std::move(automation_controller_delegate));
}

UninstallAppController::~UninstallAppController() = default;

void UninstallAppController::OnUninstallFinished() {
  DCHECK_EQ(this, instance_);

  delete this;
  instance_ = nullptr;
}

// UninstallAppController::AutomationControllerDelegate ------------------------

class UninstallAppController::AutomationControllerDelegate
    : public AutomationController::Delegate {
 public:
  AutomationControllerDelegate(
      scoped_refptr<base::SequencedTaskRunner> controller_runner,
      base::OnceClosure on_automation_finished,
      const base::string16& application_name);
  ~AutomationControllerDelegate() override;

  // AutomationController::Delegate:
  void OnInitialized(HRESULT result) const override;
  void ConfigureCacheRequest(
      IUIAutomationCacheRequest* cache_request) const override;
  void OnAutomationEvent(IUIAutomation* automation,
                         IUIAutomationElement* sender,
                         EVENTID event_id) const override;
  void OnFocusChangedEvent(IUIAutomation* automation,
                           IUIAutomationElement* sender) const override;

 private:
  // The task runner on which the UninstallAppController lives.
  scoped_refptr<base::SequencedTaskRunner> controller_runner_;

  // Protect against concurrent accesses to |on_automation_finished_|.
  mutable base::Lock on_automation_finished_lock_;

  // Called once when the automation work is done.
  mutable base::OnceClosure on_automation_finished_;

  const base::string16 application_name_;

  DISALLOW_COPY_AND_ASSIGN(AutomationControllerDelegate);
};

UninstallAppController::AutomationControllerDelegate::
    AutomationControllerDelegate(
        scoped_refptr<base::SequencedTaskRunner> controller_runner,
        base::OnceClosure on_automation_finished,
        const base::string16& application_name)
    : controller_runner_(std::move(controller_runner)),
      on_automation_finished_(std::move(on_automation_finished)),
      application_name_(application_name) {}

UninstallAppController::AutomationControllerDelegate::
    ~AutomationControllerDelegate() = default;

void UninstallAppController::AutomationControllerDelegate::OnInitialized(
    HRESULT result) const {
  // Launch the Apps & Features settings page regardless of the |result| of the
  // initialization. An initialization failure only means that the application
  // will not be written into the search box.
  ShellUtil::LaunchUninstallAppsSettings();
}

void UninstallAppController::AutomationControllerDelegate::
    ConfigureCacheRequest(IUIAutomationCacheRequest* cache_request) const {
  cache_request->AddPattern(UIA_ValuePatternId);
  cache_request->AddProperty(UIA_AutomationIdPropertyId);
  cache_request->AddProperty(UIA_IsWindowPatternAvailablePropertyId);
}

void UninstallAppController::AutomationControllerDelegate::OnAutomationEvent(
    IUIAutomation* automation,
    IUIAutomationElement* sender,
    EVENTID event_id) const {}

void UninstallAppController::AutomationControllerDelegate::OnFocusChangedEvent(
    IUIAutomation* automation,
    IUIAutomationElement* sender) const {
  base::string16 combo_box_id(
      GetCachedBstrValue(sender, UIA_AutomationIdPropertyId));
  if (combo_box_id != L"SystemSettings_AppsFeatures_AppControl_ComboBox")
    return;

  base::OnceClosure callback;
  {
    base::AutoLock auto_lock(on_automation_finished_lock_);
    callback = std::move(on_automation_finished_);
  }

  // This callback can be null if the application name was already written in
  // the search box and this instance is awaiting destruction.
  if (!callback)
    return;

  Microsoft::WRL::ComPtr<IUIAutomationElement> search_box;
  if (!FindSearchBoxElement(automation, sender, &search_box))
    return;

  Microsoft::WRL::ComPtr<IUIAutomationValuePattern> value_pattern;
  HRESULT result = search_box->GetCachedPatternAs(UIA_ValuePatternId,
                                                  IID_PPV_ARGS(&value_pattern));
  if (FAILED(result))
    return;

  CComBSTR bstr(application_name_.c_str());
  value_pattern->SetValue(bstr);

  controller_runner_->PostTask(FROM_HERE, std::move(callback));
}

}  // namespace

void LaunchUninstallFlow(const base::string16& application_name) {
  UninstallAppController::Launch(application_name);
}

}  // namespace uninstall_application
