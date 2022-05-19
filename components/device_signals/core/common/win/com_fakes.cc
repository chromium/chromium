// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/win/com_fakes.h"

#include "base/check.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device_signals {

#define IMPL_IUNKOWN_NOQI_WITH_REF(cls)                               \
  IFACEMETHODIMP cls::QueryInterface(REFIID riid, void** ppv) {       \
    return E_NOTIMPL;                                                 \
  }                                                                   \
  ULONG cls::AddRef() { return ::InterlockedIncrement(&ref_count_); } \
  ULONG cls::Release(void) {                                          \
    DCHECK(ref_count_ > 0);                                           \
    return ::InterlockedDecrement(&ref_count_);                       \
  }

IMPL_IUNKOWN_NOQI_WITH_REF(FakeWbemClassObject)

FakeEnumWbemClassObject::FakeEnumWbemClassObject() = default;

FakeEnumWbemClassObject::~FakeEnumWbemClassObject() = default;

HRESULT FakeEnumWbemClassObject::Next(long lTimeout,
                                      ULONG uCount,
                                      IWbemClassObject** apObjects,
                                      ULONG* puReturned) {
  // This fake implementation only supports moving through items one by one.
  DCHECK(uCount == 1);

  if (!iterator_.has_value()) {
    iterator_ = items_.begin();
  }

  if (iterator_.value() == items_.end()) {
    // Reached the end of the vector.
    *puReturned = 0;
    return WBEM_S_FALSE;
  }

  *puReturned = iterator_.value()->second;

  // If the next item is nullptr, then return an error code.
  HRESULT result = WBEM_S_FALSE;
  if (iterator_.value()->first) {
    result = WBEM_S_NO_ERROR;
    *apObjects = iterator_.value()->first;
  }

  ++iterator_.value();
  return result;
}

HRESULT FakeEnumWbemClassObject::Clone(IEnumWbemClassObject** ppEnum) {
  return E_NOTIMPL;
}

HRESULT FakeEnumWbemClassObject::NextAsync(ULONG uCount,
                                           IWbemObjectSink* pSink) {
  return E_NOTIMPL;
}

HRESULT FakeEnumWbemClassObject::Reset() {
  return E_NOTIMPL;
}

HRESULT FakeEnumWbemClassObject::Skip(long lTimeout, ULONG nCount) {
  return E_NOTIMPL;
}

IMPL_IUNKOWN_NOQI_WITH_REF(FakeEnumWbemClassObject)

FakeWbemClassObject::FakeWbemClassObject() = default;
FakeWbemClassObject::FakeWbemClassObject(FakeWbemClassObject&&) = default;
FakeWbemClassObject& FakeWbemClassObject::operator=(FakeWbemClassObject&&) =
    default;

FakeWbemClassObject::~FakeWbemClassObject() = default;

HRESULT FakeWbemClassObject::Get(LPCWSTR wszName,
                                 long lFlags,
                                 VARIANT* pVal,
                                 CIMTYPE* pType,
                                 long* plFlavor) {
  EXPECT_EQ(0, lFlags);

  std::wstring key(wszName);
  auto iterator = map_.find(key);
  if (iterator == map_.end()) {
    // Not found, return any HRESULT that is not 0.
    return WBEM_S_FALSE;
  }

  *pVal = iterator->second.Copy();

  return WBEM_S_NO_ERROR;
}

HRESULT FakeWbemClassObject::Delete(LPCWSTR wszName) {
  map_.erase(wszName);
  return WBEM_S_NO_ERROR;
}

HRESULT FakeWbemClassObject::BeginEnumeration(long lEnumFlags) {
  return E_NOTIMPL;
}

HRESULT FakeWbemClassObject::BeginMethodEnumeration(long lEnumFlags) {
  return E_NOTIMPL;
}

HRESULT FakeWbemClassObject::Clone(IWbemClassObject** ppCopy) {
  return E_NOTIMPL;
}

HRESULT FakeWbemClassObject::CompareTo(long lFlags,
                                       IWbemClassObject* pCompareTo) {
  return E_NOTIMPL;
}

HRESULT FakeWbemClassObject::DeleteMethod(LPCWSTR wszName) {
  return E_NOTIMPL;
}

HRESULT FakeWbemClassObject::EndEnumeration() {
  return E_NOTIMPL;
}

HRESULT FakeWbemClassObject::EndMethodEnumeration() {
  return E_NOTIMPL;
}

HRESULT FakeWbemClassObject::GetMethod(LPCWSTR wszName,
                                       long lFlags,
                                       IWbemClassObject** ppInSignature,
                                       IWbemClassObject** ppOutSignature) {
  return E_NOTIMPL;
}

HRESULT FakeWbemClassObject::GetMethodOrigin(LPCWSTR wszMethodName,
                                             BSTR* pstrClassName) {
  return E_NOTIMPL;
}

HRESULT FakeWbemClassObject::GetMethodQualifierSet(
    LPCWSTR wszMethod,
    IWbemQualifierSet** ppQualSet) {
  return E_NOTIMPL;
}

HRESULT FakeWbemClassObject::GetNames(LPCWSTR wszQualifierName,
                                      long lFlags,
                                      VARIANT* pQualifierVal,
                                      SAFEARRAY** pNames) {
  return E_NOTIMPL;
}

HRESULT FakeWbemClassObject::GetObjectText(long lFlags, BSTR* pstrObjectText) {
  return E_NOTIMPL;
}

HRESULT FakeWbemClassObject::GetPropertyOrigin(LPCWSTR wszName,
                                               BSTR* pstrClassName) {
  return E_NOTIMPL;
}

HRESULT FakeWbemClassObject::GetPropertyQualifierSet(
    LPCWSTR wszProperty,
    IWbemQualifierSet** ppQualSet) {
  return E_NOTIMPL;
}

HRESULT FakeWbemClassObject::GetQualifierSet(IWbemQualifierSet** ppQualSet) {
  return E_NOTIMPL;
}

HRESULT FakeWbemClassObject::InheritsFrom(LPCWSTR strAncestor) {
  return E_NOTIMPL;
}

HRESULT FakeWbemClassObject::Next(long lFlags,
                                  BSTR* strName,
                                  VARIANT* pVal,
                                  CIMTYPE* pType,
                                  long* plFlavor) {
  return E_NOTIMPL;
}

HRESULT FakeWbemClassObject::NextMethod(long lFlags,
                                        BSTR* pstrName,
                                        IWbemClassObject** ppInSignature,
                                        IWbemClassObject** ppOutSignature) {
  return E_NOTIMPL;
}

HRESULT FakeWbemClassObject::Put(LPCWSTR wszName,
                                 long lFlags,
                                 VARIANT* pVal,
                                 CIMTYPE Type) {
  return E_NOTIMPL;
}

HRESULT FakeWbemClassObject::PutMethod(LPCWSTR wszName,
                                       long lFlags,
                                       IWbemClassObject* pInSignature,
                                       IWbemClassObject* pOutSignature) {
  return E_NOTIMPL;
}

HRESULT FakeWbemClassObject::SpawnDerivedClass(long lFlags,
                                               IWbemClassObject** ppNewClass) {
  return E_NOTIMPL;
}

HRESULT FakeWbemClassObject::SpawnInstance(long lFlags,
                                           IWbemClassObject** ppNewInstance) {
  return E_NOTIMPL;
}

}  // namespace device_signals
