// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/chaps_util/chaps_slot_session.h"

#include <dlfcn.h>
#include <pkcs11.h>
#include <pkcs11t.h>

#include <memory>
#include <optional>

#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/scoped_blocking_call.h"

namespace chromeos {

namespace {

const char kChapsPath[] = "libchaps.so";

// Error codes used for logging. Should not be reordered or reused.
enum class ErrorCode {
  kDlopenFailed = 0,
  kFunctionListNotFound = 1,
  kFunctionListFailed = 2,
  kRequiredFunctionMissing = 3,
  kOpenSessionFailed = 4,
  kCloseSessionFailed = 5,
};

// Central logging functions that output error codes to avoid binary size bloat.
void LogError(ErrorCode error_code) {
  LOG(ERROR) << "ChapsSlotSession error=" << (int)error_code;
}

void LogError(ErrorCode error_code, CK_RV pkcs11_error) {
  LOG(ERROR) << "ChapsSlotSession error=" << (int)error_code
             << ", pkcs11_error=" << pkcs11_error;
}

struct ChapsFunctions {
  static std::optional<ChapsFunctions> GetFunctions(
      CK_FUNCTION_LIST_PTR function_list) {
    ChapsFunctions functions;
    functions.open_session = function_list->C_OpenSession;
    functions.close_session = function_list->C_CloseSession;
    functions.create_object = function_list->C_CreateObject;
    functions.generate_key_pair = function_list->C_GenerateKeyPair;
    functions.get_attribute_value = function_list->C_GetAttributeValue;
    functions.set_attribute_value = function_list->C_SetAttributeValue;

    if (functions.open_session && functions.close_session &&
        functions.create_object && functions.generate_key_pair &&
        functions.get_attribute_value && functions.set_attribute_value) {
      return functions;
    }
    return std::nullopt;
  }

  CK_C_OpenSession open_session = nullptr;
  CK_C_CloseSession close_session = nullptr;
  CK_C_CreateObject create_object = nullptr;
  CK_C_GenerateKeyPair generate_key_pair = nullptr;
  CK_C_GetAttributeValue get_attribute_value = nullptr;
  CK_C_SetAttributeValue set_attribute_value = nullptr;
};

// Default implementation of a ChapsSlotSession using the libchaps.so library.
// This implementation expects that C_Initialize has already been called for
// chaps in this process.
class ChapsSlotSessionImpl : public ChapsSlotSession {
 public:
  static std::unique_ptr<ChapsSlotSessionImpl> Create(CK_SLOT_ID slot_id) {
    void* chaps_handle =
        dlopen(kChapsPath, RTLD_LOCAL | RTLD_NOW | RTLD_DEEPBIND);
    if (!chaps_handle) {
      LogError(ErrorCode::kDlopenFailed);
      return nullptr;
    }
    CK_C_GetFunctionList get_function_list =
        (CK_C_GetFunctionList)dlsym(chaps_handle, "C_GetFunctionList");
    if (!get_function_list) {
      LogError(ErrorCode::kFunctionListNotFound);
      return nullptr;
    }
    CK_FUNCTION_LIST_PTR function_list = nullptr;
    {
      CK_RV get_function_list_result = get_function_list(&function_list);
      if (CKR_OK != get_function_list_result || !function_list) {
        LogError(ErrorCode::kFunctionListFailed, get_function_list_result);
        return nullptr;
      }
    }

    auto functions = ChapsFunctions::GetFunctions(function_list);
    if (!functions) {
      LogError(ErrorCode::kRequiredFunctionMissing);
      return nullptr;
    }

    CK_SESSION_HANDLE session_handle = CK_INVALID_HANDLE;

    // Start a new PKCS#11 session for |slot_id_|.
    CK_RV open_session_result;
    {
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::WILL_BLOCK);
      open_session_result = functions->open_session(
          slot_id, kOpenSessionFlags, /*pApplication=*/nullptr,
          /*Notify=*/nullptr, &session_handle);
    }
    if (CKR_OK != open_session_result) {
      LogError(ErrorCode::kOpenSessionFailed, open_session_result);
      return nullptr;
    }
    return base::WrapUnique(new ChapsSlotSessionImpl(chaps_handle, *functions,
                                                     slot_id, session_handle));
  }

  ~ChapsSlotSessionImpl() override {
    if (session_handle_ != CK_INVALID_HANDLE) {
      CK_RV close_session_result;
      {
        base::ScopedBlockingCall scoped_blocking_call(
            FROM_HERE, base::BlockingType::WILL_BLOCK);
        close_session_result = functions_.close_session(session_handle_);
      }
      if (close_session_result != CKR_OK) {
        LogError(ErrorCode::kCloseSessionFailed, close_session_result);
      }
    }

    if (chaps_handle_) {
      dlclose(chaps_handle_);
    }
  }

  bool ReopenSession() override {
    CK_RV close_session_result;
    {
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::WILL_BLOCK);
      close_session_result = functions_.close_session(session_handle_);
    }
    if (close_session_result != CKR_SESSION_HANDLE_INVALID &&
        close_session_result != CKR_OK) {
      LogError(ErrorCode::kCloseSessionFailed, close_session_result);
    }
    session_handle_ = CK_INVALID_HANDLE;

    CK_RV open_session_result;
    {
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::WILL_BLOCK);
      open_session_result = functions_.open_session(
          slot_id_, kOpenSessionFlags, /*pApplication=*/nullptr,
          /*Notify=*/nullptr, &session_handle_);
    }
    if (CKR_OK != open_session_result) {
      LogError(ErrorCode::kOpenSessionFailed, open_session_result);
      return false;
    }
    return true;
  }

  CK_RV CreateObject(CK_ATTRIBUTE_PTR pTemplate,
                     CK_ULONG ulCount,
                     CK_OBJECT_HANDLE_PTR phObject) override {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);
    return functions_.create_object(session_handle_, pTemplate, ulCount,
                                    phObject);
  }

  CK_RV GenerateKeyPair(CK_MECHANISM_PTR pMechanism,
                        CK_ATTRIBUTE_PTR pPublicKeyTemplate,
                        CK_ULONG ulPublicKeyAttributeCount,
                        CK_ATTRIBUTE_PTR pPrivateKeyTemplate,
                        CK_ULONG ulPrivateKeyAttributeCount,
                        CK_OBJECT_HANDLE_PTR phPublicKey,
                        CK_OBJECT_HANDLE_PTR phPrivateKey) override {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);
    return functions_.generate_key_pair(
        session_handle_, pMechanism, pPublicKeyTemplate,
        ulPublicKeyAttributeCount, pPrivateKeyTemplate,
        ulPrivateKeyAttributeCount, phPublicKey, phPrivateKey);
  }

  CK_RV GetAttributeValue(CK_OBJECT_HANDLE hObject,
                          CK_ATTRIBUTE_PTR pTemplate,
                          CK_ULONG ulCount) override {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);
    return functions_.get_attribute_value(session_handle_, hObject, pTemplate,
                                          ulCount);
  }

  CK_RV SetAttributeValue(CK_OBJECT_HANDLE hObject,
                          CK_ATTRIBUTE_PTR pTemplate,
                          CK_ULONG ulCount) override {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);
    return functions_.set_attribute_value(session_handle_, hObject, pTemplate,
                                          ulCount);
  }

 private:
  ChapsSlotSessionImpl(void* chaps_handle,
                       const ChapsFunctions& functions,
                       const CK_SLOT_ID slot_id,
                       CK_SESSION_HANDLE session_handle)
      : chaps_handle_(chaps_handle),
        functions_(functions),
        slot_id_(slot_id),
        session_handle_(session_handle) {}
  // Pass CKF_RW_SESSION in case the intention is to generate keys.
  // CKF_SERIAL_SESSION should always be set according to
  // http://docs.oasis-open.org/pkcs11/pkcs11-base/v2.40/os/pkcs11-base-v2.40-os.html#_Toc416959688
  // and chaps verifies that.
  static constexpr CK_FLAGS kOpenSessionFlags =
      CKF_RW_SESSION | CKF_SERIAL_SESSION;

  raw_ptr<void> chaps_handle_ = nullptr;
  ChapsFunctions functions_;

  const CK_SLOT_ID slot_id_;
  CK_SESSION_HANDLE session_handle_ = CK_INVALID_HANDLE;
};

}  // namespace

std::unique_ptr<ChapsSlotSession>
ChapsSlotSessionFactoryImpl::CreateChapsSlotSession(CK_SLOT_ID slot_id) {
  return ChapsSlotSessionImpl::Create(slot_id);
}

}  // namespace chromeos
