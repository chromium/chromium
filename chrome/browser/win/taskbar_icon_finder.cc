// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/taskbar_icon_finder.h"

#include <windows.h>
#include <wrl/client.h>

#include <objbase.h>
#include <oleauto.h>
#include <uiautomation.h>

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_variant.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/rect.h"

namespace {

// TaskbarIconFinder -----------------------------------------------------------

// A class that uses UIAutomation in a COM multi-threaded apartment thread to
// find the bounding rectangle of Chrome's taskbar icon on the system's primary
// monitor.
class TaskbarIconFinder {
 public:
  // Constructs a new finder and immediately starts running it on a dedicated
  // automation task in a multi-threaded COM apartment.
  explicit TaskbarIconFinder(TaskbarIconFinderResultCallback result_callback);

  TaskbarIconFinder(const TaskbarIconFinder&) = delete;
  TaskbarIconFinder& operator=(const TaskbarIconFinder&) = delete;

 private:
  // Receives the result computed on the automation task, passes the results to
  // the caller, then self-destructs.
  void OnComplete(const gfx::Rect& rect);

  // Main function for the finder's automation task. Bounces the results of
  // the operation back to OnComplete on the caller's sequenced task runner
  // (|finder_runner|).
  static void RunOnComTask(
      scoped_refptr<base::SequencedTaskRunner> finder_runner,
      TaskbarIconFinder* finder);

  // Returns the values of the |property_id| property (of type VT_R8 | VT_ARRAY)
  // cached in |element|. May only be used on the automation task.
  static std::vector<double> GetCachedDoubleArrayValue(
      IUIAutomationElement* element,
      PROPERTYID property_id);

  // Populates |rect| with the bounding rectangle of any item in |icons| that is
  // on the primary monitor. |rect| is unmodified if no such item/rect is found.
  // May only be used on the automation task.
  static void FindRectOnPrimaryMonitor(IUIAutomation* automation,
                                       IUIAutomationElementArray* icons,
                                       gfx::Rect* rect);

  // Finds an item with an automation id matching Chrome's app user model id.
  // Returns the first failure HRESULT, or the final success HRESULT. On
  // success, |rect| is populated with the bouning rectangle of the icon if
  // found.
  static HRESULT DoOnComTask(gfx::Rect* rect);

  // The caller's callback.
  TaskbarIconFinderResultCallback result_callback_;
};

TaskbarIconFinder::TaskbarIconFinder(
    TaskbarIconFinderResultCallback result_callback)
    : result_callback_(std::move(result_callback)) {
  DCHECK(result_callback_);

  // Since all threads servicing the base::ThreadPool initialize COM into the
  // MTA and only one task is needed for this job, it is sufficient to post a
  // simple task here. Should automation event handlers be needed or more than
  // one task, care must be taken to follow proper threading rules as required
  // for automation clients.
  base::ThreadPool::PostTask(
      FROM_HERE, base::BindOnce(&TaskbarIconFinder::RunOnComTask,
                                base::SequencedTaskRunner::GetCurrentDefault(),
                                base::Unretained(this)));
}

void TaskbarIconFinder::OnComplete(const gfx::Rect& rect) {
  std::move(result_callback_).Run(rect);
  delete this;
}

// static
void TaskbarIconFinder::RunOnComTask(
    scoped_refptr<base::SequencedTaskRunner> finder_runner,
    TaskbarIconFinder* finder) {
  // This and all methods below must be called on the automation task.
  DCHECK(!finder_runner->RunsTasksInCurrentSequence());

  gfx::Rect rect;
  DoOnComTask(&rect);
  finder_runner->PostTask(FROM_HERE,
                          base::BindOnce(&TaskbarIconFinder::OnComplete,
                                         base::Unretained(finder), rect));
}

// static
std::vector<double> TaskbarIconFinder::GetCachedDoubleArrayValue(
    IUIAutomationElement* element,
    PROPERTYID property_id) {
  base::win::AssertComApartmentType(base::win::ComApartmentType::MTA);

  std::vector<double> values;
  base::win::ScopedVariant var;

  if (FAILED(element->GetCachedPropertyValueEx(property_id, TRUE,
                                               var.Receive()))) {
    return values;
  }

  if (V_VT(var.ptr()) != (VT_R8 | VT_ARRAY)) {
    LOG_IF(ERROR, V_VT(var.ptr()) != VT_UNKNOWN)
        << __func__ << " property is not an R8 array: " << V_VT(var.ptr());
    return values;
  }

  SAFEARRAY* array = V_ARRAY(var.ptr());
  if (SafeArrayGetDim(array) != 1)
    return values;
  long lower_bound = 0;
  long upper_bound = 0;
  SafeArrayGetLBound(array, 1, &lower_bound);
  SafeArrayGetUBound(array, 1, &upper_bound);
  if (lower_bound || upper_bound <= lower_bound)
    return values;
  double* data = nullptr;
  SafeArrayAccessData(array, reinterpret_cast<void**>(&data));
  values.assign(data, data + upper_bound + 1);
  SafeArrayUnaccessData(array);

  return values;
}

// static
void TaskbarIconFinder::FindRectOnPrimaryMonitor(
    IUIAutomation* automation,
    IUIAutomationElementArray* icons,
    gfx::Rect* rect) {
  base::win::AssertComApartmentType(base::win::ComApartmentType::MTA);

  int length = 0;
  icons->get_Length(&length);

  // Find each icon's nearest ancestor with an HWND.
  Microsoft::WRL::ComPtr<IUIAutomationTreeWalker> tree_walker;
  HRESULT result = automation->get_RawViewWalker(&tree_walker);
  if (FAILED(result) || !tree_walker)
    return;
  Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> cache_request;
  result = automation->CreateCacheRequest(&cache_request);
  if (FAILED(result) || !cache_request)
    return;
  cache_request->AddProperty(UIA_NativeWindowHandlePropertyId);

  Microsoft::WRL::ComPtr<IUIAutomationElement> icon;
  HWND hwnd = 0;
  for (int i = 0; i < length; ++i) {
    icons->GetElement(i, &icon);

    // Walk up the tree to find the icon's first parent with an HWND.
    Microsoft::WRL::ComPtr<IUIAutomationElement> search = icon;
    while (true) {
      Microsoft::WRL::ComPtr<IUIAutomationElement> parent;
      result = tree_walker->GetParentElementBuildCache(
          search.Get(), cache_request.Get(), &parent);
      if (FAILED(result) || !parent)
        break;
      base::win::ScopedVariant var;
      result = parent->GetCachedPropertyValueEx(
          UIA_NativeWindowHandlePropertyId, TRUE, var.Receive());
      if (FAILED(result))
        break;
      hwnd = reinterpret_cast<HWND>(V_I4(var.ptr()));
      if (hwnd)
        break;  // Found.
      search.Reset();
      std::swap(parent, search);
    }

    // No parent hwnd found for this icon.
    if (!hwnd)
      continue;

    // Is this icon's window on the primary monitor?
    HMONITOR monitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONULL);
    MONITORINFO monitor_info = {};
    monitor_info.cbSize = sizeof(monitor_info);
    if (monitor && ::GetMonitorInfo(monitor, &monitor_info) &&
        (monitor_info.dwFlags & MONITORINFOF_PRIMARY) != 0) {
      break;  // All done.
    }
    icon.Reset();
  }

  if (!icon)
    return;  // No taskbar icon found on the primary monitor.

  std::vector<double> bounding_rect =
      GetCachedDoubleArrayValue(icon.Get(), UIA_BoundingRectanglePropertyId);
  if (!bounding_rect.empty()) {
    gfx::Rect screen_rect(bounding_rect[0], bounding_rect[1], bounding_rect[2],
                          bounding_rect[3]);
    *rect = display::win::ScreenWin::ScreenToDIPRect(hwnd, screen_rect);
  }
}

// static
HRESULT TaskbarIconFinder::DoOnComTask(gfx::Rect* rect) {
  base::win::AssertComApartmentType(base::win::ComApartmentType::MTA);

  Microsoft::WRL::ComPtr<IUIAutomation> automation;
  HRESULT result =
      ::CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&automation));
  if (FAILED(result) || !automation)
    return result;

  // Create a condition: automation_id=ap_user_model_id && type=button_type.
  base::win::ScopedVariant app_user_model_id(
      ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall()).c_str());
  Microsoft::WRL::ComPtr<IUIAutomationCondition> id_condition;
  result = automation->CreatePropertyCondition(
      UIA_AutomationIdPropertyId, app_user_model_id, &id_condition);
  if (FAILED(result) || !id_condition)
    return result;

  base::win::ScopedVariant button_type(UIA_ButtonControlTypeId);
  Microsoft::WRL::ComPtr<IUIAutomationCondition> type_condition;
  result = automation->CreatePropertyCondition(UIA_ControlTypePropertyId,
                                               button_type, &type_condition);
  if (FAILED(result) || !type_condition)
    return result;

  Microsoft::WRL::ComPtr<IUIAutomationCondition> condition;
  result = automation->CreateAndCondition(id_condition.Get(),
                                          type_condition.Get(), &condition);

  // Cache the bounding rectangle of all found items.
  Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> cache_request;
  result = automation->CreateCacheRequest(&cache_request);
  if (FAILED(result) || !cache_request)
    return result;
  cache_request->AddProperty(UIA_BoundingRectanglePropertyId);

  // Search the desktop to find all buttons with the correct automation id.
  Microsoft::WRL::ComPtr<IUIAutomationElement> desktop;
  result = automation->GetRootElement(&desktop);
  if (FAILED(result) || !desktop)
    return result;

  Microsoft::WRL::ComPtr<IUIAutomationElementArray> icons;
  result = desktop->FindAllBuildCache(TreeScope_Subtree, condition.Get(),
                                      cache_request.Get(), &icons);
  if (FAILED(result) || !icons)
    return result;

  // Pick the icon on the primary monitor.
  FindRectOnPrimaryMonitor(automation.Get(), icons.Get(), rect);
  return S_OK;
}

}  // namespace

void FindTaskbarIcon(TaskbarIconFinderResultCallback result_callback) {
  DCHECK(result_callback);
  // The instance self-destructs in OnComplete.
  new TaskbarIconFinder(std::move(result_callback));
}
