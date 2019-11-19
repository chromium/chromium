// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/scoped_lsa_policy.h"

#include <Windows.h>  // Needed for ACCESS_MASK, <lm.h>
#include <Winternl.h>
#include <lm.h>  // Needed for LSA_UNICODE_STRING

#define _NTDEF_  // Prevent redefition errors, must come after <winternl.h>
#include <ntsecapi.h>  // For LSA_xxx types

#include "chrome/credential_provider/gaiacp/gcp_utils.h"  // For STATUS_SUCCESS.
#include "chrome/credential_provider/gaiacp/logging.h"

namespace credential_provider {

// static
ScopedLsaPolicy::CreatorCallback* ScopedLsaPolicy::GetCreatorCallbackStorage() {
  static CreatorCallback creator_for_testing;
  return &creator_for_testing;
}

// static
void ScopedLsaPolicy::SetCreatorForTesting(CreatorCallback creator) {
  *GetCreatorCallbackStorage() = creator;
}

// static
std::unique_ptr<ScopedLsaPolicy> ScopedLsaPolicy::Create(ACCESS_MASK mask) {
  if (!GetCreatorCallbackStorage()->is_null())
    return GetCreatorCallbackStorage()->Run(mask);

  std::unique_ptr<ScopedLsaPolicy> scoped(new ScopedLsaPolicy(mask));
  return scoped->IsValid() ? std::move(scoped) : nullptr;
}

ScopedLsaPolicy::ScopedLsaPolicy(ACCESS_MASK mask) {
  LSA_OBJECT_ATTRIBUTES oa;
  memset(&oa, 0, sizeof(oa));
  NTSTATUS sts = ::LsaOpenPolicy(nullptr, &oa, mask, &handle_);
  if (sts != STATUS_SUCCESS) {
    HRESULT hr = HRESULT_FROM_NT(sts);
    LOGFN(ERROR) << "LsaOpenPolicy hr=" << putHR(hr);
    ::SetLastError(hr);
    handle_ = nullptr;
  }
}

ScopedLsaPolicy::~ScopedLsaPolicy() {
  if (handle_ != nullptr)
    ::LsaClose(handle_);
}

bool ScopedLsaPolicy::IsValid() const {
  return handle_ != nullptr;
}

HRESULT ScopedLsaPolicy::StorePrivateData(const wchar_t* key,
                                          const wchar_t* value) {
  LSA_UNICODE_STRING lsa_key;
  InitLsaString(key, &lsa_key);
  LSA_UNICODE_STRING lsa_value;
  InitLsaString(value, &lsa_value);

  // When calling LsaStorePrivateData(), the value's length should include
  // the null terminator.
  lsa_value.Length = lsa_value.MaximumLength;

  NTSTATUS sts = ::LsaStorePrivateData(handle_, &lsa_key, &lsa_value);
  if (sts != STATUS_SUCCESS) {
    HRESULT hr = HRESULT_FROM_NT(sts);
    LOGFN(ERROR) << "LsaStorePrivateData hr=" << putHR(hr);
    return hr;
  }
  return S_OK;
}

HRESULT ScopedLsaPolicy::RemovePrivateData(const wchar_t* key) {
  LSA_UNICODE_STRING lsa_key;
  InitLsaString(key, &lsa_key);

  NTSTATUS sts = ::LsaStorePrivateData(handle_, &lsa_key, nullptr);
  if (sts != STATUS_SUCCESS) {
    HRESULT hr = HRESULT_FROM_NT(sts);
    LOGFN(ERROR) << "LsaStorePrivateData hr=" << putHR(hr);
    return hr;
  }
  return S_OK;
}

HRESULT ScopedLsaPolicy::RetrievePrivateData(const wchar_t* key,
                                             wchar_t* value,
                                             size_t length) {
  LSA_UNICODE_STRING lsa_key;
  InitLsaString(key, &lsa_key);
  LSA_UNICODE_STRING* lsa_value;

  NTSTATUS sts = ::LsaRetrievePrivateData(handle_, &lsa_key, &lsa_value);
  if (sts != STATUS_SUCCESS)
    return HRESULT_FROM_NT(sts);

  errno_t err = wcscpy_s(value, length, lsa_value->Buffer);
  SecurelyClearBuffer(lsa_value->Buffer, lsa_value->Length);
  ::LsaFreeMemory(lsa_value);

  return err == 0 ? S_OK : E_FAIL;
}

bool ScopedLsaPolicy::PrivateDataExists(const wchar_t* key) {
  LSA_UNICODE_STRING lsa_key;
  InitLsaString(key, &lsa_key);
  LSA_UNICODE_STRING* lsa_value;

  NTSTATUS sts = ::LsaRetrievePrivateData(handle_, &lsa_key, &lsa_value);

  if (sts != STATUS_SUCCESS)
    return false;

  SecurelyClearBuffer(lsa_value->Buffer, lsa_value->Length);
  ::LsaFreeMemory(lsa_value);

  return true;
}

HRESULT ScopedLsaPolicy::AddAccountRights(PSID sid, const wchar_t* right) {
  LSA_UNICODE_STRING lsa_right;
  InitLsaString(right, &lsa_right);
  NTSTATUS sts = ::LsaAddAccountRights(handle_, sid, &lsa_right, 1);
  if (sts != STATUS_SUCCESS) {
    HRESULT hr = HRESULT_FROM_NT(sts);
    LOGFN(ERROR) << "LsaAddAccountRights sts=" << putHR(sts)
                 << " hr=" << putHR(hr);
    return hr;
  }
  return S_OK;
}

HRESULT ScopedLsaPolicy::RemoveAccount(PSID sid) {
  // When all rights are removed from an account, the account itself is also
  // deleted.
  // I thought the above meant the user would be removed from the
  // computer, but apparently I am mistaken.  It is still important to call
  // NetUserDel().
  NTSTATUS sts = ::LsaRemoveAccountRights(handle_, sid, TRUE, nullptr, 0);
  if (sts != STATUS_SUCCESS) {
    HRESULT hr = HRESULT_FROM_NT(sts);
    LOGFN(ERROR) << "LsaRemoveAccountRights sts=" << putHR(sts)
                 << " hr=" << putHR(hr);
    return hr;
  }
  return S_OK;
}

// static
void ScopedLsaPolicy::InitLsaString(const wchar_t* string,
                                    _UNICODE_STRING* lsa_string) {
  lsa_string->Buffer = const_cast<wchar_t*>(string);
  lsa_string->Length =
      static_cast<USHORT>(wcslen(lsa_string->Buffer) * sizeof(wchar_t));
  lsa_string->MaximumLength = lsa_string->Length + sizeof(wchar_t);
}

}  // namespace credential_provider
