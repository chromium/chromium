// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_UI_AUTOMATION_UTIL_H_
#define CHROME_BROWSER_WIN_UI_AUTOMATION_UTIL_H_

// This header defines some utility functions that facilitates the usage of
// the UIAutomation API in Chrome. Primarily used by AutomationController and
// its users.

// Must be before <uiautomation.h>
#include <objbase.h>

#include <string>
#include <vector>

#include <uiautomation.h>

// Returns a cached BSTR property of |element|.
std::wstring GetCachedBstrValue(IUIAutomationElement* element,
                                PROPERTYID property_id);

// Debug utilities. They are not used in release builds to avoid adding a lot of
// unnecessary strings into executable. If DCHECK are disabled, these are dummy
// functions that do nothing useful.
bool GetCachedBoolValue(IUIAutomationElement* element, PROPERTYID property_id);

int32_t GetCachedInt32Value(IUIAutomationElement* element,
                            PROPERTYID property_id);

std::vector<int32_t> GetCachedInt32ArrayValue(IUIAutomationElement* element,
                                              PROPERTYID property_id);

std::string IntArrayToString(const std::vector<int32_t>& values);

const char* GetEventName(EVENTID event_id);

const char* GetControlType(long control_type);

#endif  // CHROME_BROWSER_WIN_UI_AUTOMATION_UTIL_H_
