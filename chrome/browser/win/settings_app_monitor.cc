// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/settings_app_monitor.h"

#include <wrl/client.h>

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/pattern.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/win/scoped_variant.h"
#include "chrome/browser/win/automation_controller.h"
#include "chrome/browser/win/ui_automation_util.h"
#include "chrome/common/chrome_features.h"

namespace {

// Each item represent one UI element in the Settings App.
enum class ElementType {
  // The "Web browser" element in the "Default apps" pane.
  DEFAULT_BROWSER,
  // The element representing a browser in the "Choose an app" popup.
  BROWSER_BUTTON,
  // The button labeled "Check it out" that leaves Edge as the default browser.
  CHECK_IT_OUT,
  // The button labeled "Switch Anyway" that dismisses the Edge promo.
  SWITCH_ANYWAY,
  // Any other element.
  UNKNOWN,
};

// Configures a cache request so that it includes all properties needed by
// DetectElementType() to detect the elements of interest.
void ConfigureCacheRequest(IUIAutomationCacheRequest* cache_request) {
  DCHECK(cache_request);
  cache_request->AddProperty(UIA_AutomationIdPropertyId);
  cache_request->AddProperty(UIA_NamePropertyId);
  cache_request->AddProperty(UIA_ClassNamePropertyId);
  cache_request->AddPattern(UIA_InvokePatternId);
}

// Helper function to get the parent element with class name "Flyout". Used to
// determine the |element|'s type.
base::string16 GetFlyoutParentAutomationId(IUIAutomation* automation,
                                           IUIAutomationElement* element) {
  // Create a condition that will include only elements with the right class
  // name in the tree view.
  base::win::ScopedVariant class_name(L"Flyout");
  Microsoft::WRL::ComPtr<IUIAutomationCondition> condition;
  HRESULT result = automation->CreatePropertyCondition(
      UIA_ClassNamePropertyId, class_name, condition.GetAddressOf());
  if (FAILED(result))
    return base::string16();

  Microsoft::WRL::ComPtr<IUIAutomationTreeWalker> tree_walker;
  result =
      automation->CreateTreeWalker(condition.Get(), tree_walker.GetAddressOf());
  if (FAILED(result))
    return base::string16();

  Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> cache_request;
  result = automation->CreateCacheRequest(cache_request.GetAddressOf());
  if (FAILED(result))
    return base::string16();
  ConfigureCacheRequest(cache_request.Get());

  // From MSDN, NormalizeElementBuildCache() "Retrieves the ancestor element
  // nearest to the specified Microsoft UI Automation element in the tree view".
  IUIAutomationElement* flyout_element = nullptr;
  result = tree_walker->NormalizeElementBuildCache(element, cache_request.Get(),
                                                   &flyout_element);
  if (FAILED(result) || !flyout_element)
    return base::string16();

  return GetCachedBstrValue(flyout_element, UIA_AutomationIdPropertyId);
}

ElementType DetectElementType(IUIAutomation* automation,
                              IUIAutomationElement* sender) {
  DCHECK(automation);
  DCHECK(sender);
  base::string16 aid(GetCachedBstrValue(sender, UIA_AutomationIdPropertyId));
  if (aid == L"SystemSettings_DefaultApps_Browser_Button")
    return ElementType::DEFAULT_BROWSER;
  if (aid == L"SystemSettings_DefaultApps_Browser_App0_HyperlinkButton")
    return ElementType::SWITCH_ANYWAY;
  if (base::MatchPattern(aid, L"SystemSettings_DefaultApps_Browser_*_Button")) {
    // This element type depends on the automation id of one of its ancestors.
    base::string16 automation_id =
        GetFlyoutParentAutomationId(automation, sender);
    if (automation_id == L"settingsFlyout")
      return ElementType::CHECK_IT_OUT;
    else if (automation_id == L"DefaultAppsFlyoutPresenter")
      return ElementType::BROWSER_BUTTON;
  }
  return ElementType::UNKNOWN;
}

}  // namespace

class SettingsAppMonitor::AutomationControllerDelegate
    : public AutomationController::Delegate {
 public:
  AutomationControllerDelegate(
      scoped_refptr<base::SequencedTaskRunner> monitor_runner,
      base::WeakPtr<SettingsAppMonitor> monitor);
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
  // Invokes the |browser_button| if the Win10AcceleratedDefaultBrowserFlow
  // feature is enabled.
  void MaybeInvokeChooser(IUIAutomationElement* browser_button) const;

  // The task runner on which the SettingsAppMonitor lives.
  const scoped_refptr<base::SequencedTaskRunner> monitor_runner_;

  // Only used to post callbacks to |monitor_runner_|;
  const base::WeakPtr<SettingsAppMonitor> monitor_;

  // Protect against concurrent accesses to |last_focused_element_|.
  mutable base::Lock last_focused_element_lock_;

  // State to suppress duplicate "focus changed" events.
  mutable ElementType last_focused_element_;

  // Protect against concurrent accesses to |browser_chooser_invoked_|.
  mutable base::Lock browser_chooser_invoked_lock_;

  // The browser chooser must only be invoked once.
  mutable bool browser_chooser_invoked_;

  DISALLOW_COPY_AND_ASSIGN(AutomationControllerDelegate);
};

SettingsAppMonitor::AutomationControllerDelegate::AutomationControllerDelegate(
    scoped_refptr<base::SequencedTaskRunner> monitor_runner,
    base::WeakPtr<SettingsAppMonitor> monitor)
    : monitor_runner_(monitor_runner),
      monitor_(std::move(monitor)),
      last_focused_element_(ElementType::UNKNOWN),
      browser_chooser_invoked_(false) {}

SettingsAppMonitor::AutomationControllerDelegate::
    ~AutomationControllerDelegate() = default;

void SettingsAppMonitor::AutomationControllerDelegate::OnInitialized(
    HRESULT result) const {
  monitor_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&SettingsAppMonitor::OnInitialized, monitor_, result));
}

void SettingsAppMonitor::AutomationControllerDelegate::ConfigureCacheRequest(
    IUIAutomationCacheRequest* cache_request) const {
  ::ConfigureCacheRequest(cache_request);
}

void SettingsAppMonitor::AutomationControllerDelegate::OnAutomationEvent(
    IUIAutomation* automation,
    IUIAutomationElement* sender,
    EVENTID event_id) const {
  switch (DetectElementType(automation, sender)) {
    case ElementType::DEFAULT_BROWSER:
      monitor_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&SettingsAppMonitor::OnChooserInvoked, monitor_));
      break;
    case ElementType::BROWSER_BUTTON: {
      base::string16 browser_name(
          GetCachedBstrValue(sender, UIA_NamePropertyId));
      if (!browser_name.empty()) {
        monitor_runner_->PostTask(
            FROM_HERE, base::BindOnce(&SettingsAppMonitor::OnBrowserChosen,
                                      monitor_, browser_name));
      }
      break;
    }
    case ElementType::SWITCH_ANYWAY:
      monitor_runner_->PostTask(
          FROM_HERE, base::BindOnce(&SettingsAppMonitor::OnPromoChoiceMade,
                                    monitor_, false));
      break;
    case ElementType::CHECK_IT_OUT:
      monitor_runner_->PostTask(
          FROM_HERE, base::BindOnce(&SettingsAppMonitor::OnPromoChoiceMade,
                                    monitor_, true));
      break;
    case ElementType::UNKNOWN:
      break;
  }
}

void SettingsAppMonitor::AutomationControllerDelegate::OnFocusChangedEvent(
    IUIAutomation* automation,
    IUIAutomationElement* sender) const {
  ElementType element_type = DetectElementType(automation, sender);
  {
    // Duplicate focus changed events are suppressed.
    base::AutoLock auto_lock(last_focused_element_lock_);
    if (last_focused_element_ == element_type)
      return;
    last_focused_element_ = element_type;
  }

  if (element_type == ElementType::DEFAULT_BROWSER) {
    MaybeInvokeChooser(sender);
    monitor_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SettingsAppMonitor::OnAppFocused, monitor_));
  } else if (element_type == ElementType::CHECK_IT_OUT) {
    monitor_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SettingsAppMonitor::OnPromoFocused, monitor_));
  }
}

void SettingsAppMonitor::AutomationControllerDelegate::MaybeInvokeChooser(
    IUIAutomationElement* browser_button) const {
  if (!base::FeatureList::IsEnabled(
          features::kWin10AcceleratedDefaultBrowserFlow)) {
    return;
  }

  {
    // Only invoke the browser chooser once.
    base::AutoLock auto_lock(browser_chooser_invoked_lock_);
    if (browser_chooser_invoked_)
      return;
    browser_chooser_invoked_ = true;
  }

  // Invoke the dialog and record whether it was successful.
  Microsoft::WRL::ComPtr<IUIAutomationInvokePattern> invoke_pattern;
  bool succeeded = SUCCEEDED(browser_button->GetCachedPatternAs(
                       UIA_InvokePatternId, IID_PPV_ARGS(&invoke_pattern))) &&
                   invoke_pattern && SUCCEEDED(invoke_pattern->Invoke());

  UMA_HISTOGRAM_BOOLEAN("DefaultBrowser.Win10ChooserInvoked", succeeded);
}

SettingsAppMonitor::SettingsAppMonitor(Delegate* delegate)
    : delegate_(delegate) {
  // A fully initialized WeakPtrFactory is needed to create the
  // AutomationControllerDelegate.
  auto automation_controller_delegate =
      std::make_unique<SettingsAppMonitor::AutomationControllerDelegate>(
          base::SequencedTaskRunnerHandle::Get(),
          weak_ptr_factory_.GetWeakPtr());

  automation_controller_ = std::make_unique<AutomationController>(
      std::move(automation_controller_delegate));
}

SettingsAppMonitor::~SettingsAppMonitor() = default;

void SettingsAppMonitor::OnInitialized(HRESULT result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnInitialized(result);
}

void SettingsAppMonitor::OnAppFocused() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnAppFocused();
}

void SettingsAppMonitor::OnChooserInvoked() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnChooserInvoked();
}

void SettingsAppMonitor::OnBrowserChosen(const base::string16& browser_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnBrowserChosen(browser_name);
}

void SettingsAppMonitor::OnPromoFocused() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnPromoFocused();
}

void SettingsAppMonitor::OnPromoChoiceMade(bool accept_promo) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnPromoChoiceMade(accept_promo);
}
