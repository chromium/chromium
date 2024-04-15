// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/window_contents_as_string_win.h"

// Needed for <uiautomation.h>
#include <objbase.h>

#include <wrl/client.h>

#include <utility>

#include "base/check.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"

#include <uiautomation.h>

namespace {

// IDs corresponding to the properties read by CachedElementContentsAsString.
constexpr long kCachedProperties[] = {
    UIA_LocalizedControlTypePropertyId,
    UIA_NamePropertyId,
    UIA_IsInvokePatternAvailablePropertyId,
};

// Returns a string representation of the cached properties of |element|.
std::wstring CachedElementContentsAsString(IUIAutomationElement* element) {
  std::wstring contents;

  base::win::ScopedVariant variant;
  HRESULT result = element->GetCachedPropertyValue(
      UIA_IsInvokePatternAvailablePropertyId, variant.Receive());
  if (SUCCEEDED(result) && variant.type() == VT_BOOL &&
      V_BOOL(variant.ptr()) == VARIANT_TRUE) {
    contents.append(L"[invokable] ");
  }

  base::win::ScopedBstr value;
  result = element->get_CachedLocalizedControlType(value.Receive());
  if (SUCCEEDED(result))
    contents.append(L"type: ").append(value.Get());

  value.Reset();
  result = element->get_CachedName(value.Receive());
  if (SUCCEEDED(result)) {
    if (!contents.empty())
      contents.append(L", ");
    contents.append(L"name: ").append(value.Get());
  }

  return contents;
}

void TreeAsString(IUIAutomationTreeWalker* walker,
                  IUIAutomationCacheRequest* cache_request,
                  IUIAutomationElement* element,
                  const std::wstring& padding,
                  std::wstring* contents) {
  contents->append(padding)
      .append(CachedElementContentsAsString(element))
      .append(L"\n");

  Microsoft::WRL::ComPtr<IUIAutomationElement> child_element;
  HRESULT result = walker->GetFirstChildElementBuildCache(
      element, cache_request, &child_element);
  if (FAILED(result))
    return;
  const std::wstring next_padding = padding + L"  ";
  while (child_element.Get()) {
    TreeAsString(walker, cache_request, child_element.Get(), next_padding,
                 contents);
    Microsoft::WRL::ComPtr<IUIAutomationElement> next_element;
    result = walker->GetNextSiblingElementBuildCache(
        child_element.Get(), cache_request, &next_element);
    if (FAILED(result))
      return;
    child_element = std::move(next_element);
  }
}

}  // namespace

std::wstring WindowContentsAsString(HWND window_handle) {
  DCHECK(window_handle);
  base::win::AssertComInitialized();

  Microsoft::WRL::ComPtr<IUIAutomation> automation;

  HRESULT result =
      ::CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&automation));
  if (FAILED(result))
    return std::wstring();

  Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> cache_request;
  result = automation->CreateCacheRequest(&cache_request);
  if (FAILED(result))
    return std::wstring();
  for (auto property_id : kCachedProperties)
    cache_request->AddProperty(property_id);

  Microsoft::WRL::ComPtr<IUIAutomationElement> window_element;
  result = automation->ElementFromHandleBuildCache(
      window_handle, cache_request.Get(), &window_element);
  if (FAILED(result))
    return std::wstring();

  Microsoft::WRL::ComPtr<IUIAutomationTreeWalker> walker;
  result = automation->get_RawViewWalker(&walker);
  if (FAILED(result))
    return std::wstring();

  std::wstring contents;
  TreeAsString(walker.Get(), cache_request.Get(), window_element.Get(),
               std::wstring(), &contents);
  // Strip the trailing newline.
  if (!contents.empty())
    contents.pop_back();
  return contents;
}
