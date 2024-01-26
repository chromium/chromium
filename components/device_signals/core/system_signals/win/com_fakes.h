// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_WIN_COM_FAKES_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_WIN_COM_FAKES_H_

#include <atlcomcli.h>
#include <iwscapi.h>
#include <wbemidl.h>

#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"

namespace device_signals {

#define DECLARE_IUNKOWN_NOQI_WITH_REF()                            \
  IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override; \
  ULONG STDMETHODCALLTYPE AddRef() override;                       \
  ULONG STDMETHODCALLTYPE Release(void) override;                  \
  ULONG ref_count_ = 1;

#define DECLARE_IDISPATCH()                                                   \
  DECLARE_IUNKOWN_NOQI_WITH_REF()                                             \
  IFACEMETHODIMP GetTypeInfoCount(UINT* pctinfo) override;                    \
  IFACEMETHODIMP GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo)     \
      override;                                                               \
  IFACEMETHODIMP GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames, \
                               LCID lcid, DISPID* rgDispId) override;         \
  IFACEMETHODIMP Invoke(DISPID dispIdMember, REFIID riid, LCID lcid,          \
                        WORD wFlags, DISPPARAMS* pDispParams,                 \
                        VARIANT* pVarResult, EXCEPINFO* pExcepInfo,           \
                        UINT* puArgErr) override;

class FakeEnumWbemClassObject : public IEnumWbemClassObject {
 public:
  FakeEnumWbemClassObject();

  virtual ~FakeEnumWbemClassObject();

  void Add(IWbemClassObject* class_object, ULONG items_returned = 1) {
    items_.emplace_back(class_object, items_returned);
  }

 private:
  // IEnumWbemClassObject:
  DECLARE_IUNKOWN_NOQI_WITH_REF()
  IFACEMETHODIMP Clone(IEnumWbemClassObject** ppEnum) override;
  IFACEMETHODIMP Next(long lTimeout,
                      ULONG uCount,
                      IWbemClassObject** apObjects,
                      ULONG* puReturned) override;
  IFACEMETHODIMP NextAsync(ULONG uCount, IWbemObjectSink* pSink) override;
  IFACEMETHODIMP Reset() override;
  IFACEMETHODIMP Skip(long lTimeout, ULONG nCount) override;

  std::vector<std::pair<IWbemClassObject*, ULONG>> items_;
  std::optional<std::vector<std::pair<IWbemClassObject*, ULONG>>::iterator>
      iterator_;
};

class FakeWbemClassObject : public IWbemClassObject {
 public:
  FakeWbemClassObject();

  FakeWbemClassObject(const FakeWbemClassObject& copy) = delete;
  FakeWbemClassObject& operator=(const FakeWbemClassObject&) = delete;
  FakeWbemClassObject(FakeWbemClassObject&&);
  FakeWbemClassObject& operator=(FakeWbemClassObject&&);

  virtual ~FakeWbemClassObject();

  void Set(const wchar_t* key, const std::wstring& bstr_value) {
    map_[key] = base::win::ScopedVariant(bstr_value.data(), bstr_value.size());
  }

  void Set(const wchar_t* key, const LONG long_value) {
    map_[key] = base::win::ScopedVariant(long_value);
  }

  // Implemented IWbemClassObject:
  IFACEMETHODIMP Get(LPCWSTR wszName,
                     long lFlags,
                     VARIANT* pVal,
                     CIMTYPE* pType = nullptr,
                     long* plFlavor = nullptr) override;
  IFACEMETHODIMP Delete(LPCWSTR wszName) override;

 private:
  // IWbemClassObject:
  DECLARE_IUNKOWN_NOQI_WITH_REF()
  IFACEMETHODIMP BeginEnumeration(long lEnumFlags) override;
  IFACEMETHODIMP BeginMethodEnumeration(long lEnumFlags) override;
  IFACEMETHODIMP Clone(IWbemClassObject** ppCopy) override;
  IFACEMETHODIMP CompareTo(long lFlags, IWbemClassObject* pCompareTo) override;
  IFACEMETHODIMP DeleteMethod(LPCWSTR wszName) override;
  IFACEMETHODIMP EndEnumeration() override;
  IFACEMETHODIMP EndMethodEnumeration() override;
  IFACEMETHODIMP GetMethod(LPCWSTR wszName,
                           long lFlags,
                           IWbemClassObject** ppInSignature,
                           IWbemClassObject** ppOutSignature) override;
  IFACEMETHODIMP GetMethodOrigin(LPCWSTR wszMethodName,
                                 BSTR* pstrClassName) override;
  IFACEMETHODIMP GetMethodQualifierSet(LPCWSTR wszMethod,
                                       IWbemQualifierSet** ppQualSet) override;
  IFACEMETHODIMP GetNames(LPCWSTR wszQualifierName,
                          long lFlags,
                          VARIANT* pQualifierVal,
                          SAFEARRAY** pNames) override;
  IFACEMETHODIMP GetObjectText(long lFlags, BSTR* pstrObjectText) override;
  IFACEMETHODIMP GetPropertyOrigin(LPCWSTR wszName,
                                   BSTR* pstrClassName) override;
  IFACEMETHODIMP GetPropertyQualifierSet(
      LPCWSTR wszProperty,
      IWbemQualifierSet** ppQualSet) override;
  IFACEMETHODIMP GetQualifierSet(IWbemQualifierSet** ppQualSet) override;
  IFACEMETHODIMP InheritsFrom(LPCWSTR strAncestor) override;
  IFACEMETHODIMP Next(long lFlags,
                      BSTR* strName,
                      VARIANT* pVal,
                      CIMTYPE* pType = nullptr,
                      long* plFlavor = nullptr) override;
  IFACEMETHODIMP NextMethod(long lFlags,
                            BSTR* pstrName,
                            IWbemClassObject** ppInSignature,
                            IWbemClassObject** ppOutSignature) override;
  IFACEMETHODIMP Put(LPCWSTR wszName,
                     long lFlags,
                     VARIANT* pVal,
                     CIMTYPE Type) override;
  IFACEMETHODIMP PutMethod(LPCWSTR wszName,
                           long lFlags,
                           IWbemClassObject* pInSignature,
                           IWbemClassObject* pOutSignature) override;
  IFACEMETHODIMP SpawnDerivedClass(long lFlags,
                                   IWbemClassObject** ppNewClass) override;
  IFACEMETHODIMP SpawnInstance(long lFlags,
                               IWbemClassObject** ppNewInstance) override;

  std::map<std::wstring, base::win::ScopedVariant> map_;
};

class FakeWscProduct : public IWscProduct {
 public:
  FakeWscProduct();
  FakeWscProduct(const wchar_t* name,
                 const wchar_t* id,
                 WSC_SECURITY_PRODUCT_STATE state);

  FakeWscProduct(const FakeWscProduct& copy) = delete;
  FakeWscProduct& operator=(const FakeWscProduct&) = delete;
  FakeWscProduct(FakeWscProduct&&);
  FakeWscProduct& operator=(FakeWscProduct&&);

  virtual ~FakeWscProduct();

  enum class FailureStep {
    kProductName = 0,
    kProductId = 1,
    kProductState = 2,
  };

  // IWscProduct:
  IFACEMETHODIMP get_ProductName(BSTR* pVal) override;
  IFACEMETHODIMP get_ProductGuid(BSTR* pVal) override;
  IFACEMETHODIMP get_ProductState(WSC_SECURITY_PRODUCT_STATE* pVal) override;

  // Can be used to force a failure to happen in one of the functions
  // represented by `step`.
  void set_failed_step(FailureStep step) { failed_step_ = step; }

 private:
  // IWscProduct:
  DECLARE_IDISPATCH()
  IFACEMETHODIMP get_ProductStateTimestamp(BSTR* pVal) override;
  IFACEMETHODIMP get_RemediationPath(BSTR* pVal) override;
  IFACEMETHODIMP get_SignatureStatus(
      WSC_SECURITY_SIGNATURE_STATUS* pVal) override;
  IFACEMETHODIMP get_ProductIsDefault(BOOL* pVal) override;

  bool ShouldFail(FailureStep step);

  std::optional<FailureStep> failed_step_;

  base::win::ScopedBstr name_;
  base::win::ScopedBstr id_;
  WSC_SECURITY_PRODUCT_STATE state_;
};

class FakeWSCProductList : public IWSCProductList {
 public:
  FakeWSCProductList();

  virtual ~FakeWSCProductList();

  enum class FailureStep {
    kInitialize = 0,
    kGetCount = 1,
    kGetItem = 2,
  };

  void Add(IWscProduct* product) { products_.push_back(product); }
  // IWSCProductList:
  IFACEMETHODIMP get_Count(LONG* pVal) override;
  IFACEMETHODIMP get_Item(ULONG index, IWscProduct** pVal) override;
  IFACEMETHODIMP Initialize(ULONG provider) override;

  // Can be used to force a failure to happen in one of the functions
  // represented by `step`.
  void set_failed_step(FailureStep step) { failed_step_ = step; }

  const std::optional<ULONG>& provider() { return provider_; }

 private:
  // IWSCProductList:
  DECLARE_IDISPATCH()

  bool ShouldFail(FailureStep step);

  std::optional<FailureStep> failed_step_;

  std::optional<ULONG> provider_;
  std::vector<raw_ptr<IWscProduct, VectorExperimental>> products_;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_WIN_COM_FAKES_H_
