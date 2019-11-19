// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/window_contents_as_string_win.h"

// Needed for <uiautomation.h>
#include <objbase.h>

#include <uiautomation.h>
#include <wrl/client.h>

#include <utility>

#include "base/logging.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"

namespace {

// IDs corresponding to the properties read by CachedElementContentsAsString.
constexpr long kCachedProperties[] = {
    UIA_LocalizedControlTypePropertyId,
    UIA_NamePropertyId,
    UIA_IsInvokePatternAvailablePropertyId,
};

// Returns a string representation of the cached properties of |element|.
base::string16 CachedElementContentsAsString(IUIAutomationElement* element) {
  base::string16 contents;

  base::win::ScopedVariant variant;
  HRESULT result = element->GetCachedPropertyValue(
      UIA_IsInvokePatternAvailablePropertyId, variant.Receive());
  if (SUCCEEDED(result) && variant.type() == VT_BOOL &&
      V_BOOL(variant.ptr()) == VARIANT_TRUE) {
    contents.append(STRING16_LITERAL("[invokable] "));
  }

  base::win::ScopedBstr value;
  result = element->get_CachedLocalizedControlType(value.Receive());
  if (SUCCEEDED(result))
    contents.append(STRING16_LITERAL("type: ")).append(value);

  value.Reset();
  result = element->get_CachedName(value.Receive());
  if (SUCCEEDED(result)) {
    if (!contents.empty())
      contents.append(STRING16_LITERAL(", "));
    contents.append(STRING16_LITERAL("name: ")).append(value);
  }

  return contents;
}

void TreeAsString(IUIAutomationTreeWalker* walker,
                  IUIAutomationCacheRequest* cache_request,
                  IUIAutomationElement* element,
                  const base::string16& padding,
                  base::string16* contents) {
  contents->append(padding)
      .append(CachedElementContentsAsString(element))
      .append(STRING16_LITERAL("\n"));

  Microsoft::WRL::ComPtr<IUIAutomationElement> child_element;
  HRESULT result = walker->GetFirstChildElementBuildCache(
      element, cache_request, &child_element);
  if (FAILED(result))
    return;
  const base::string16 next_padding = padding + STRING16_LITERAL("  ");
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

base::string16 WindowContentsAsString(HWND window_handle) {
  DCHECK(window_handle);
  base::win::AssertComInitialized();

  Microsoft::WRL::ComPtr<IUIAutomation> automation;

  HRESULT result =
      ::CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&automation));
  if (FAILED(result))
    return base::string16();

  Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> cache_request;
  result = automation->CreateCacheRequest(&cache_request);
  if (FAILED(result))
    return base::string16();
  for (auto property_id : kCachedProperties)
    cache_request->AddProperty(property_id);

  Microsoft::WRL::ComPtr<IUIAutomationElement> window_element;
  result = automation->ElementFromHandleBuildCache(
      window_handle, cache_request.Get(), &window_element);
  if (FAILED(result))
    return base::string16();

  Microsoft::WRL::ComPtr<IUIAutomationTreeWalker> walker;
  result = automation->get_RawViewWalker(&walker);
  if (FAILED(result))
    return base::string16();

  base::string16 contents;
  TreeAsString(walker.Get(), cache_request.Get(), window_element.Get(),
               base::string16(), &contents);
  // Strip the trailing newline.
  if (!contents.empty())
    contents.pop_back();
  return contents;
}
