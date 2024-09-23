// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/settings_app_monitor.h"

#include <wrl/client.h>

#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/win/scoped_variant.h"
#include "chrome/browser/win/automation_controller.h"
#include "chrome/browser/win/ui_automation_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/installer/util/install_util.h"

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
  // The button labeled "Set Default" in the browser Default Apps sub-page.
  SET_AS_DEFAULT_BROWSER,
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
std::wstring GetFlyoutParentAutomationId(IUIAutomation* automation,
                                         IUIAutomationElement* element) {
  // Create a condition that will include only elements with the right class
  // name in the tree view.
  base::win::ScopedVariant class_name(L"Flyout");
  Microsoft::WRL::ComPtr<IUIAutomationCondition> condition;
  HRESULT result = automation->CreatePropertyCondition(UIA_ClassNamePropertyId,
                                                       class_name, &condition);
  if (FAILED(result))
    return std::wstring();

  Microsoft::WRL::ComPtr<IUIAutomationTreeWalker> tree_walker;
  result = automation->CreateTreeWalker(condition.Get(), &tree_walker);
  if (FAILED(result))
    return std::wstring();

  Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> cache_request;
  result = automation->CreateCacheRequest(&cache_request);
  if (FAILED(result))
    return std::wstring();
  ConfigureCacheRequest(cache_request.Get());

  // From MSDN, NormalizeElementBuildCache() "Retrieves the ancestor element
  // nearest to the specified Microsoft UI Automation element in the tree view".
  IUIAutomationElement* flyout_element = nullptr;
  result = tree_walker->NormalizeElementBuildCache(element, cache_request.Get(),
                                                   &flyout_element);
  if (FAILED(result) || !flyout_element)
    return std::wstring();

  return GetCachedBstrValue(flyout_element, UIA_AutomationIdPropertyId);
}

ElementType DetectElementType(IUIAutomation* automation,
                              IUIAutomationElement* sender) {
  DCHECK(automation);
  DCHECK(sender);
  std::wstring aid(GetCachedBstrValue(sender, UIA_AutomationIdPropertyId));
  // Win 11 "Set Default" button on Apps > Default Apps > <Browser> page.
  if (aid == L"SystemSettings_DefaultApps_DefaultBrowserAction_Button") {
    return ElementType::SET_AS_DEFAULT_BROWSER;
  }
  if (aid == L"SystemSettings_DefaultApps_Browser_Button")
    return ElementType::DEFAULT_BROWSER;
  if (aid == L"SystemSettings_DefaultApps_Browser_App0_HyperlinkButton")
    return ElementType::SWITCH_ANYWAY;
  if (base::MatchPattern(base::AsString16(aid),
                         u"SystemSettings_DefaultApps_Browser_*_Button")) {
    // This element type depends on the automation id of one of its ancestors.
    std::wstring automation_id =
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

  AutomationControllerDelegate(const AutomationControllerDelegate&) = delete;
  AutomationControllerDelegate& operator=(const AutomationControllerDelegate&) =
      delete;

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

  // Protect against concurrent access to `set_as_default_clicked_`.
  mutable base::Lock set_as_default_clicked_lock_;

  // Only react to the first time set as default is clicked.
  mutable bool set_as_default_clicked_;
};

SettingsAppMonitor::AutomationControllerDelegate::AutomationControllerDelegate(
    scoped_refptr<base::SequencedTaskRunner> monitor_runner,
    base::WeakPtr<SettingsAppMonitor> monitor)
    : monitor_runner_(monitor_runner),
      monitor_(std::move(monitor)),
      last_focused_element_(ElementType::UNKNOWN),
      browser_chooser_invoked_(false),
      set_as_default_clicked_(false) {}

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
      std::wstring browser_name(GetCachedBstrValue(sender, UIA_NamePropertyId));
      if (!browser_name.empty()) {
        monitor_runner_->PostTask(
            FROM_HERE, base::BindOnce(&SettingsAppMonitor::OnBrowserChosen,
                                      monitor_, browser_name));
      }
      break;
    }
    case ElementType::SET_AS_DEFAULT_BROWSER: {
      // Multiple events are generated for clicking on Set as default. Only
      // pay attention to the first one.
      base::AutoLock auto_lock(set_as_default_clicked_lock_);
      if (set_as_default_clicked_) {
        break;
      }
      set_as_default_clicked_ = true;

      std::wstring browser_name(GetCachedBstrValue(sender, UIA_NamePropertyId));
      // `browser_name` will be "Make <browser name> your default browser" so if
      // we find ourselves in `browser_name`, pass that to OnBrowserChosen. This
      // won't be perfect if user brings up the dialog with Google Chrome but
      // navigates around and sets Google Chrome Beta as the default browser,
      // but that should be rare, and is OK.
      if (browser_name.find(InstallUtil::GetDisplayName()) !=
          std::wstring::npos) {
        browser_name = InstallUtil::GetDisplayName();
      }
      monitor_runner_->PostTask(
          FROM_HERE, base::BindOnce(&SettingsAppMonitor::OnBrowserChosen,
                                    monitor_, browser_name));
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
  browser_button->GetCachedPatternAs(UIA_InvokePatternId,
                                     IID_PPV_ARGS(&invoke_pattern));
  if (!invoke_pattern) {
    return;
  }
  invoke_pattern->Invoke();
}

SettingsAppMonitor::SettingsAppMonitor(Delegate* delegate)
    : delegate_(delegate) {
  // A fully initialized WeakPtrFactory is needed to create the
  // AutomationControllerDelegate.
  auto automation_controller_delegate =
      std::make_unique<SettingsAppMonitor::AutomationControllerDelegate>(
          base::SequencedTaskRunner::GetCurrentDefault(),
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

void SettingsAppMonitor::OnBrowserChosen(const std::wstring& browser_name) {
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
