// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IMPORTER_PSTORE_DECLARATIONS_H_
#define CHROME_COMMON_IMPORTER_PSTORE_DECLARATIONS_H_

#ifdef __PSTORE_H__
#error Should not include pstore.h and this file simultaneously.
#endif

#include <ole2.h>

// pstore.h is no longer shipped in the Windows 8 SDK. Define a minimal set
// here.

// These types are referenced in interfaces we use, but our code does not use
// refer to these types, so simply make them opaque.
class IEnumPStoreTypes;
struct PST_ACCESSRULESET;
struct PST_PROMPTINFO;
struct PST_PROVIDERINFO;
struct PST_TYPEINFO;

EXTERN_C const IID IID_IPStore;
EXTERN_C const IID IID_IEnumPStoreItems;

typedef DWORD PST_KEY;
typedef DWORD PST_ACCESSMODE;
#define PST_E_OK _HRESULT_TYPEDEF_(0x00000000L)

interface
#ifdef __clang__
    [[clang::lto_visibility_public]]
#endif
    IEnumPStoreItems : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE Next(
      DWORD celt,
      LPWSTR __RPC_FAR *rgelt,
      DWORD __RPC_FAR *pceltFetched) = 0;

  virtual HRESULT STDMETHODCALLTYPE Skip(DWORD celt) = 0;

  virtual HRESULT STDMETHODCALLTYPE Reset(void) = 0;

  virtual HRESULT STDMETHODCALLTYPE Clone(
      IEnumPStoreItems __RPC_FAR *__RPC_FAR *ppenum) = 0;
};

interface
#ifdef __clang__
    [[clang::lto_visibility_public]]
#endif
    IPStore : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE GetInfo(
      PST_PROVIDERINFO* __RPC_FAR *ppProperties) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetProvParam(
      DWORD dwParam,
      DWORD __RPC_FAR *pcbData,
      BYTE __RPC_FAR *__RPC_FAR *ppbData,
      DWORD dwFlags) = 0;

  virtual HRESULT STDMETHODCALLTYPE SetProvParam(
      DWORD dwParam,
      DWORD cbData,
      BYTE __RPC_FAR *pbData,
      DWORD dwFlags) = 0;

  virtual HRESULT STDMETHODCALLTYPE CreateType(
      PST_KEY Key,
      const GUID __RPC_FAR *pType,
      PST_TYPEINFO* pInfo,
      DWORD dwFlags) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetTypeInfo(
      PST_KEY Key,
      const GUID __RPC_FAR *pType,
      PST_TYPEINFO* __RPC_FAR *ppInfo,
      DWORD dwFlags) = 0;

  virtual HRESULT STDMETHODCALLTYPE DeleteType(
      PST_KEY Key,
      const GUID __RPC_FAR *pType,
      DWORD dwFlags) = 0;

  virtual HRESULT STDMETHODCALLTYPE CreateSubtype(
      PST_KEY Key,
      const GUID __RPC_FAR *pType,
      const GUID __RPC_FAR *pSubtype,
      PST_TYPEINFO* pInfo,
      PST_ACCESSRULESET* pRules,
      DWORD dwFlags) = 0;

  virtual HRESULT STDMETHODCALLTYPE GetSubtypeInfo(
      PST_KEY Key,
      const GUID __RPC_FAR *pType,
      const GUID __RPC_FAR *pSubtype,
      PST_TYPEINFO* __RPC_FAR *ppInfo,
      DWORD dwFlags) = 0;

  virtual HRESULT STDMETHODCALLTYPE DeleteSubtype(
      PST_KEY Key,
      const GUID __RPC_FAR *pType,
      const GUID __RPC_FAR *pSubtype,
      DWORD dwFlags) = 0;

  virtual HRESULT STDMETHODCALLTYPE ReadAccessRuleset(
      PST_KEY Key,
      const GUID __RPC_FAR *pType,
      const GUID __RPC_FAR *pSubtype,
      PST_ACCESSRULESET* __RPC_FAR *ppRules,
      DWORD dwFlags) = 0;

  virtual HRESULT STDMETHODCALLTYPE WriteAccessRuleset(
      PST_KEY Key,
      const GUID __RPC_FAR *pType,
      const GUID __RPC_FAR *pSubtype,
      PST_ACCESSRULESET* pRules,
      DWORD dwFlags) = 0;

  virtual HRESULT STDMETHODCALLTYPE EnumTypes(
      PST_KEY Key,
      DWORD dwFlags,
      IEnumPStoreTypes __RPC_FAR *__RPC_FAR *ppenum) = 0;

  virtual HRESULT STDMETHODCALLTYPE EnumSubtypes(
      PST_KEY Key,
      const GUID __RPC_FAR *pType,
      DWORD dwFlags,
      IEnumPStoreTypes __RPC_FAR *__RPC_FAR *ppenum) = 0;

  virtual HRESULT STDMETHODCALLTYPE DeleteItem(
      PST_KEY Key,
      const GUID __RPC_FAR *pItemType,
      const GUID __RPC_FAR *pItemSubtype,
      LPCWSTR szItemName,
      PST_PROMPTINFO* pPromptInfo,
      DWORD dwFlags) = 0;

  virtual HRESULT STDMETHODCALLTYPE ReadItem(
      PST_KEY Key,
      const GUID __RPC_FAR *pItemType,
      const GUID __RPC_FAR *pItemSubtype,
      LPCWSTR szItemName,
      DWORD __RPC_FAR *pcbData,
      BYTE __RPC_FAR *__RPC_FAR *ppbData,
      PST_PROMPTINFO* pPromptInfo,
      DWORD dwFlags) = 0;

  virtual HRESULT STDMETHODCALLTYPE WriteItem(
      PST_KEY Key,
      const GUID __RPC_FAR *pItemType,
      const GUID __RPC_FAR *pItemSubtype,
      LPCWSTR szItemName,
      DWORD cbData,
      BYTE __RPC_FAR *pbData,
      PST_PROMPTINFO* pPromptInfo,
      DWORD dwDefaultConfirmationStyle,
      DWORD dwFlags) = 0;

  virtual HRESULT STDMETHODCALLTYPE OpenItem(
      PST_KEY Key,
      const GUID __RPC_FAR *pItemType,
      const GUID __RPC_FAR *pItemSubtype,
      LPCWSTR szItemName,
      PST_ACCESSMODE ModeFlags,
      PST_PROMPTINFO* pPromptInfo,
      DWORD dwFlags) = 0;

  virtual HRESULT STDMETHODCALLTYPE CloseItem(
      PST_KEY Key,
      const GUID __RPC_FAR *pItemType,
      const GUID __RPC_FAR *pItemSubtype,
      LPCWSTR szItemName,
      DWORD dwFlags) = 0;

  virtual HRESULT STDMETHODCALLTYPE EnumItems(
      PST_KEY Key,
      const GUID __RPC_FAR *pItemType,
      const GUID __RPC_FAR *pItemSubtype,
      DWORD dwFlags,
      IEnumPStoreItems __RPC_FAR *__RPC_FAR *ppenum) = 0;
};

#endif  // CHROME_COMMON_IMPORTER_PSTORE_DECLARATIONS_H_
