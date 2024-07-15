// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/ui_automation_util.h"

#include <iterator>

#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_safearray.h"
#include "base/win/scoped_variant.h"

std::wstring GetCachedBstrValue(IUIAutomationElement* element,
                                PROPERTYID property_id) {
  HRESULT result = S_OK;
  base::win::ScopedVariant var;

  result = element->GetCachedPropertyValueEx(property_id, TRUE, var.Receive());
  if (FAILED(result))
    return std::wstring();

  if (V_VT(var.ptr()) != VT_BSTR) {
    LOG_IF(ERROR, V_VT(var.ptr()) != VT_UNKNOWN)
        << __func__ << " property is not a BSTR: " << V_VT(var.ptr());
    return std::wstring();
  }

  return std::wstring(V_BSTR(var.ptr()));
}

bool GetCachedBoolValue(IUIAutomationElement* element, PROPERTYID property_id) {
#if DCHECK_IS_ON()
  base::win::ScopedVariant var;

  if (FAILED(element->GetCachedPropertyValueEx(property_id, TRUE,
                                               var.Receive()))) {
    return false;
  }

  if (V_VT(var.ptr()) != VT_BOOL) {
    LOG_IF(ERROR, V_VT(var.ptr()) != VT_UNKNOWN)
        << __func__ << " property is not a BOOL: " << V_VT(var.ptr());
    return false;
  }

  return V_BOOL(var.ptr()) != 0;
#else   // DCHECK_IS_ON()
  return false;
#endif  // !DCHECK_IS_ON()
}

int32_t GetCachedInt32Value(IUIAutomationElement* element,
                            PROPERTYID property_id) {
#if DCHECK_IS_ON()
  base::win::ScopedVariant var;

  if (FAILED(element->GetCachedPropertyValueEx(property_id, TRUE,
                                               var.Receive()))) {
    return false;
  }

  if (V_VT(var.ptr()) != VT_I4) {
    LOG_IF(ERROR, V_VT(var.ptr()) != VT_UNKNOWN)
        << __func__ << " property is not an I4: " << V_VT(var.ptr());
    return false;
  }

  return V_I4(var.ptr());
#else   // DCHECK_IS_ON()
  return 0;
#endif  // !DCHECK_IS_ON()
}

std::vector<int32_t> GetCachedInt32ArrayValue(IUIAutomationElement* element,
                                              PROPERTYID property_id) {
  std::vector<int32_t> values;
#if DCHECK_IS_ON()
  base::win::ScopedVariant var;

  if (FAILED(element->GetCachedPropertyValueEx(property_id, TRUE,
                                               var.Receive()))) {
    return values;
  }

  if (V_VT(var.ptr()) != (VT_I4 | VT_ARRAY)) {
    LOG_IF(ERROR, V_VT(var.ptr()) != VT_UNKNOWN)
        << __func__ << " property is not an I4 array: " << V_VT(var.ptr());
    return values;
  }

  // Convert to ScopedSafearray for convenient access to the data.
  base::win::ScopedSafearray scoped_array(var.Release().parray);

  auto lock_scope = scoped_array.CreateLockScope<VT_I4>();
  if (!lock_scope) {
    return values;
  }

  values.assign(lock_scope->begin(), lock_scope->end());
#endif  // DCHECK_IS_ON()
  return values;
}

std::string IntArrayToString(const std::vector<int32_t>& values) {
#if DCHECK_IS_ON()
  std::vector<std::string> value_strings;
  base::ranges::transform(
      values, std::back_inserter(value_strings),
      [](int32_t value) { return base::NumberToString(value); });
  return base::JoinString(value_strings, ", ");
#else   // DCHECK_IS_ON()
  return std::string();
#endif  // !DCHECK_IS_ON()
}

const char* GetEventName(EVENTID event_id) {
#if DCHECK_IS_ON()
  switch (event_id) {
    case UIA_ToolTipOpenedEventId:
      return "UIA_ToolTipOpenedEventId";
    case UIA_ToolTipClosedEventId:
      return "UIA_ToolTipClosedEventId";
    case UIA_StructureChangedEventId:
      return "UIA_StructureChangedEventId";
    case UIA_MenuOpenedEventId:
      return "UIA_MenuOpenedEventId";
    case UIA_AutomationPropertyChangedEventId:
      return "UIA_AutomationPropertyChangedEventId";
    case UIA_AutomationFocusChangedEventId:
      return "UIA_AutomationFocusChangedEventId";
    case UIA_AsyncContentLoadedEventId:
      return "UIA_AsyncContentLoadedEventId";
    case UIA_MenuClosedEventId:
      return "UIA_MenuClosedEventId";
    case UIA_LayoutInvalidatedEventId:
      return "UIA_LayoutInvalidatedEventId";
    case UIA_Invoke_InvokedEventId:
      return "UIA_Invoke_InvokedEventId";
    case UIA_SelectionItem_ElementAddedToSelectionEventId:
      return "UIA_SelectionItem_ElementAddedToSelectionEventId";
    case UIA_SelectionItem_ElementRemovedFromSelectionEventId:
      return "UIA_SelectionItem_ElementRemovedFromSelectionEventId";
    case UIA_SelectionItem_ElementSelectedEventId:
      return "UIA_SelectionItem_ElementSelectedEventId";
    case UIA_Selection_InvalidatedEventId:
      return "UIA_Selection_InvalidatedEventId";
    case UIA_Text_TextSelectionChangedEventId:
      return "UIA_Text_TextSelectionChangedEventId";
    case UIA_Text_TextChangedEventId:
      return "UIA_Text_TextChangedEventId";
    case UIA_Window_WindowOpenedEventId:
      return "UIA_Window_WindowOpenedEventId";
    case UIA_Window_WindowClosedEventId:
      return "UIA_Window_WindowClosedEventId";
    case UIA_MenuModeStartEventId:
      return "UIA_MenuModeStartEventId";
    case UIA_MenuModeEndEventId:
      return "UIA_MenuModeEndEventId";
    case UIA_InputReachedTargetEventId:
      return "UIA_InputReachedTargetEventId";
    case UIA_InputReachedOtherElementEventId:
      return "UIA_InputReachedOtherElementEventId";
    case UIA_InputDiscardedEventId:
      return "UIA_InputDiscardedEventId";
    case UIA_SystemAlertEventId:
      return "UIA_SystemAlertEventId";
    case UIA_LiveRegionChangedEventId:
      return "UIA_LiveRegionChangedEventId";
    case UIA_HostedFragmentRootsInvalidatedEventId:
      return "UIA_HostedFragmentRootsInvalidatedEventId";
    case UIA_Drag_DragStartEventId:
      return "UIA_Drag_DragStartEventId";
    case UIA_Drag_DragCancelEventId:
      return "UIA_Drag_DragCancelEventId";
    case UIA_Drag_DragCompleteEventId:
      return "UIA_Drag_DragCompleteEventId";
    case UIA_DropTarget_DragEnterEventId:
      return "UIA_DropTarget_DragEnterEventId";
    case UIA_DropTarget_DragLeaveEventId:
      return "UIA_DropTarget_DragLeaveEventId";
    case UIA_DropTarget_DroppedEventId:
      return "UIA_DropTarget_DroppedEventId";
    case UIA_TextEdit_TextChangedEventId:
      return "UIA_TextEdit_TextChangedEventId";
    case UIA_TextEdit_ConversionTargetChangedEventId:
      return "UIA_TextEdit_ConversionTargetChangedEventId";
    case UIA_ActiveTextPositionChangedEventId:
      return "UIA_ActiveTextPositionChangedEventId";
  }
#endif  // DCHECK_IS_ON()
  return "";
}

const char* GetControlType(long control_type) {
#if DCHECK_IS_ON()
  switch (control_type) {
    case UIA_ButtonControlTypeId:
      return "UIA_ButtonControlTypeId";
    case UIA_CalendarControlTypeId:
      return "UIA_CalendarControlTypeId";
    case UIA_CheckBoxControlTypeId:
      return "UIA_CheckBoxControlTypeId";
    case UIA_ComboBoxControlTypeId:
      return "UIA_ComboBoxControlTypeId";
    case UIA_EditControlTypeId:
      return "UIA_EditControlTypeId";
    case UIA_HyperlinkControlTypeId:
      return "UIA_HyperlinkControlTypeId";
    case UIA_ImageControlTypeId:
      return "UIA_ImageControlTypeId";
    case UIA_ListItemControlTypeId:
      return "UIA_ListItemControlTypeId";
    case UIA_ListControlTypeId:
      return "UIA_ListControlTypeId";
    case UIA_MenuControlTypeId:
      return "UIA_MenuControlTypeId";
    case UIA_MenuBarControlTypeId:
      return "UIA_MenuBarControlTypeId";
    case UIA_MenuItemControlTypeId:
      return "UIA_MenuItemControlTypeId";
    case UIA_ProgressBarControlTypeId:
      return "UIA_ProgressBarControlTypeId";
    case UIA_RadioButtonControlTypeId:
      return "UIA_RadioButtonControlTypeId";
    case UIA_ScrollBarControlTypeId:
      return "UIA_ScrollBarControlTypeId";
    case UIA_SliderControlTypeId:
      return "UIA_SliderControlTypeId";
    case UIA_SpinnerControlTypeId:
      return "UIA_SpinnerControlTypeId";
    case UIA_StatusBarControlTypeId:
      return "UIA_StatusBarControlTypeId";
    case UIA_TabControlTypeId:
      return "UIA_TabControlTypeId";
    case UIA_TabItemControlTypeId:
      return "UIA_TabItemControlTypeId";
    case UIA_TextControlTypeId:
      return "UIA_TextControlTypeId";
    case UIA_ToolBarControlTypeId:
      return "UIA_ToolBarControlTypeId";
    case UIA_ToolTipControlTypeId:
      return "UIA_ToolTipControlTypeId";
    case UIA_TreeControlTypeId:
      return "UIA_TreeControlTypeId";
    case UIA_TreeItemControlTypeId:
      return "UIA_TreeItemControlTypeId";
    case UIA_CustomControlTypeId:
      return "UIA_CustomControlTypeId";
    case UIA_GroupControlTypeId:
      return "UIA_GroupControlTypeId";
    case UIA_ThumbControlTypeId:
      return "UIA_ThumbControlTypeId";
    case UIA_DataGridControlTypeId:
      return "UIA_DataGridControlTypeId";
    case UIA_DataItemControlTypeId:
      return "UIA_DataItemControlTypeId";
    case UIA_DocumentControlTypeId:
      return "UIA_DocumentControlTypeId";
    case UIA_SplitButtonControlTypeId:
      return "UIA_SplitButtonControlTypeId";
    case UIA_WindowControlTypeId:
      return "UIA_WindowControlTypeId";
    case UIA_PaneControlTypeId:
      return "UIA_PaneControlTypeId";
    case UIA_HeaderControlTypeId:
      return "UIA_HeaderControlTypeId";
    case UIA_HeaderItemControlTypeId:
      return "UIA_HeaderItemControlTypeId";
    case UIA_TableControlTypeId:
      return "UIA_TableControlTypeId";
    case UIA_TitleBarControlTypeId:
      return "UIA_TitleBarControlTypeId";
    case UIA_SeparatorControlTypeId:
      return "UIA_SeparatorControlTypeId";
    case UIA_SemanticZoomControlTypeId:
      return "UIA_SemanticZoomControlTypeId";
    case UIA_AppBarControlTypeId:
      return "UIA_AppBarControlTypeId";
  }
#endif  // DCHECK_IS_ON()
  return "";
}
