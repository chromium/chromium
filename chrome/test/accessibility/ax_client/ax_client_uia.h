// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_ACCESSIBILITY_AX_CLIENT_AX_CLIENT_UIA_H_
#define CHROME_TEST_ACCESSIBILITY_AX_CLIENT_AX_CLIENT_UIA_H_

#include <stdint.h>
#include <wrl/client.h>

#include <map>
#include <ostream>
#include <string>
#include <string_view>

#include "base/containers/heap_array.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/to_string.h"
#include "chrome/test/accessibility/ax_client/ax_client_impl.h"

struct IUIAutomation;
struct IUIAutomationCacheRequest;
struct IUIAutomationElement;
struct IUIAutomationStructureChangedEventHandler;

namespace ax_client {

class AxClientUia : public AxClientImpl {
 public:
  AxClientUia();
  ~AxClientUia() override;

  // AxClientImpl:
  HRESULT Initialize(HWND hwnd) override;
  HRESULT FindAll() override;
  void Shutdown() override;

 private:
  // A bucket of properties that are cached when discovering elements.
  struct ElementProperties {
    ElementProperties(base::HeapArray<int32_t> runtime_id,
                      std::wstring automation_id,
                      std::wstring class_name,
                      std::wstring name);
    ~ElementProperties();

    base::HeapArray<int32_t> runtime_id;
    std::wstring automation_id;
    std::wstring class_name;
    std::wstring name;

    friend std::ostream& operator<<(std::ostream& out,
                                    const ElementProperties& props) {
      out << base::ToString(props.name) << " ("
          << base::ToString(props.automation_id) << ", "
          << base::ToString(props.runtime_id.as_span())
          << "): " << base::ToString(props.class_name);
      return out;
    }
  };

  struct HeapArrayLess {
    bool operator()(const base::HeapArray<int32_t>& lhs,
                    const base::HeapArray<int32_t>& rhs) const {
      return lhs.as_span() < rhs.as_span();
    }
  };

  // Adds `element` to the client's collection of discovered elements.
  void AddElement(Microsoft::WRL::ComPtr<IUIAutomationElement> element,
                  std::string_view context);

  // Removes the element with `runtime_id` from the client's collection of
  // discovered elements.
  void RemoveElement(const base::HeapArray<int32_t>& runtime_id,
                     std::string_view context);

  // Event handlers.
  void HandleStructureChangedEvent(
      Microsoft::WRL::ComPtr<IUIAutomationElement> sender,
      int change_type,
      base::HeapArray<int32_t> runtime_id);

  // Returns the value of a VT_I4 | VT_ARRAY cached property of `element`.
  static base::HeapArray<int32_t> GetIntArrayProperty(
      IUIAutomationElement* element,
      int property_id);

  // Returns the value of a VT_BSTR cached property of `element`.
  static std::wstring GetStringProperty(IUIAutomationElement* element,
                                        int property_id);

  // Returns a bucket of the cached properties of `element`.
  static ElementProperties GetProperties(IUIAutomationElement* element);

  // The connection to UIA.
  Microsoft::WRL::ComPtr<IUIAutomation> ui_automation_;

  // A cache request for general operations, including the set of properties
  // returned by `GetProperties()`.
  Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> cache_request_;

  // The top-level browser window on which this client operates.
  Microsoft::WRL::ComPtr<IUIAutomationElement> browser_window_;

  // The runtime id of the top-level browser window.
  base::HeapArray<int32_t> browser_runtime_id_;

  // Event handlers.
  Microsoft::WRL::ComPtr<IUIAutomationStructureChangedEventHandler>
      structure_changed_handler_;

  // The elements discovered by this client.
  std::map<base::HeapArray<int32_t>,
           Microsoft::WRL::ComPtr<IUIAutomationElement>,
           HeapArrayLess>
      elements_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<AxClientUia> weak_ptr_factory_{this};
};

}  // namespace ax_client

#endif  // CHROME_TEST_ACCESSIBILITY_AX_CLIENT_AX_CLIENT_UIA_H_
