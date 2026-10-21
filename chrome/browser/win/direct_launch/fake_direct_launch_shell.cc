// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/direct_launch/fake_direct_launch_shell.h"

#include <objbase.h>

#include <windows.h>

#include <propsys.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/win/propvarutil.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_propvariant.h"
#include "ui/gfx/win/singleton_hwnd.h"

namespace direct_launch {

namespace {

// The shell's direct launch API. Declared independently of
// `DirectLaunchManager`, so that a mistake in the manager's declaration fails
// the tests instead of being mirrored by this fake.
class __declspec(uuid("6fb3e48c-ba77-4f80-b9c9-77635a08a7f8"))
IExperimentalAPIInvoker : public IUnknown {
 public:
  virtual HRESULT STDMETHODCALLTYPE IsSupported(PCWSTR method_name,
                                                BOOL* is_supported) = 0;
  virtual HRESULT STDMETHODCALLTYPE Invoke(PCWSTR method_name,
                                           INamedPropertyStore* args,
                                           BOOL* is_supported) = 0;
};

// CLSID_ExperimentalAPIInvoker.
constexpr CLSID kExperimentalApiInvokerClsid = {
    0x81AF2611,
    0xE262,
    0x4090,
    {0xA1, 0x5B, 0xC0, 0x88, 0x95, 0xB1, 0xA0, 0xF0}};

// The names of the arguments of the registrations.
constexpr wchar_t kTargetWindowArg[] = L"targetWindow";
constexpr wchar_t kSchemesArg[] = L"schemes";
constexpr wchar_t kCopyDataFormatIdArg[] = L"copyDataFormatId";
constexpr wchar_t kMessageIdArg[] = L"messageId";

constexpr VARTYPE kStringVectorType = VT_VECTOR | VT_LPWSTR;

bool IsApiMethod(std::wstring_view method) {
  return method == FakeDirectLaunchShell::kRegisterUriLaunch ||
         method == FakeDirectLaunchShell::kUnregisterUriLaunch ||
         method == FakeDirectLaunchShell::kRegisterTileLaunch ||
         method == FakeDirectLaunchShell::kUnregisterTileLaunch;
}

// Reads the argument named `name` from `args` into `value`. Fails unless the
// argument is of exactly `type`, so that the tests catch the manager encoding
// an argument differently than the shell expects.
HRESULT GetArg(INamedPropertyStore* args,
               const wchar_t* name,
               VARTYPE type,
               base::win::ScopedPropVariant* value) {
  HRESULT hr = args->GetNamedValue(name, value->Receive());
  if (FAILED(hr)) {
    return hr;
  }
  return value->get().vt == type ? S_OK : E_INVALIDARG;
}

HRESULT GetWindowArg(INamedPropertyStore* args, HWND* window) {
  base::win::ScopedPropVariant value;
  HRESULT hr = GetArg(args, kTargetWindowArg, VT_I8, &value);
  if (FAILED(hr)) {
    return hr;
  }
  LONGLONG handle = 0;
  hr = ::PropVariantToInt64(value.get(), &handle);
  if (FAILED(hr)) {
    return hr;
  }
  *window = reinterpret_cast<HWND>(static_cast<intptr_t>(handle));
  return S_OK;
}

HRESULT GetUInt32Arg(INamedPropertyStore* args,
                     const wchar_t* name,
                     uint32_t* number) {
  base::win::ScopedPropVariant value;
  HRESULT hr = GetArg(args, name, VT_UI4, &value);
  if (FAILED(hr)) {
    return hr;
  }
  ULONG result = 0;
  hr = ::PropVariantToUInt32(value.get(), &result);
  if (FAILED(hr)) {
    return hr;
  }
  *number = result;
  return S_OK;
}

HRESULT GetStringVectorArg(INamedPropertyStore* args,
                           const wchar_t* name,
                           std::vector<std::wstring>* strings) {
  base::win::ScopedPropVariant value;
  HRESULT hr = GetArg(args, name, kStringVectorType, &value);
  if (FAILED(hr)) {
    return hr;
  }
  std::vector<std::wstring> result;
  const ULONG count = ::PropVariantGetElementCount(value.get());
  for (ULONG i = 0; i < count; ++i) {
    base::win::ScopedCoMem<wchar_t> element;
    hr = ::PropVariantGetStringElem(value.get(), i, &element);
    if (FAILED(hr)) {
      return hr;
    }
    result.emplace_back(element.get());
  }
  *strings = std::move(result);
  return S_OK;
}

HRESULT ReadUriRegistration(INamedPropertyStore* args,
                            FakeDirectLaunchShell::UriRegistration* result) {
  if (!args) {
    return E_POINTER;
  }
  FakeDirectLaunchShell::UriRegistration registration;
  HRESULT hr = GetWindowArg(args, &registration.target_window);
  if (FAILED(hr)) {
    return hr;
  }
  hr = GetStringVectorArg(args, kSchemesArg, &registration.schemes);
  if (FAILED(hr)) {
    return hr;
  }
  hr = GetUInt32Arg(args, kCopyDataFormatIdArg,
                    &registration.copy_data_format_id);
  if (FAILED(hr)) {
    return hr;
  }
  *result = std::move(registration);
  return S_OK;
}

HRESULT ReadTileRegistration(INamedPropertyStore* args,
                             FakeDirectLaunchShell::TileRegistration* result) {
  if (!args) {
    return E_POINTER;
  }
  FakeDirectLaunchShell::TileRegistration registration;
  HRESULT hr = GetWindowArg(args, &registration.target_window);
  if (FAILED(hr)) {
    return hr;
  }
  uint32_t message_id = 0;
  hr = GetUInt32Arg(args, kMessageIdArg, &message_id);
  if (FAILED(hr)) {
    return hr;
  }
  registration.message_id = message_id;
  *result = registration;
  return S_OK;
}

// Delivers `message` to `window`, like the shell does. The manager only
// registers the window of gfx::SingletonHwnd, so this hands the message to its
// handler directly rather than through ::SendMessage(): gfx::SingletonHwnd has
// no window at all in a test process that first used it off a UI thread.
void SendToWindow(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  gfx::SingletonHwnd* singleton_hwnd = gfx::SingletonHwnd::GetInstance();
  CHECK_EQ(window, singleton_hwnd->hwnd());
  LRESULT result = 0;
  singleton_hwnd->ProcessWindowMessage(window, message, wparam, lparam, result,
                                       /*msg_map_id=*/0);
}

}  // namespace

// The API object that the class factory hands out. It outlives the fake when
// its client keeps it, in which case it fails every invocation.
class FakeDirectLaunchShell::Invoker
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IExperimentalAPIInvoker> {
 public:
  explicit Invoker(base::WeakPtr<FakeDirectLaunchShell> shell)
      : shell_(std::move(shell)) {}
  Invoker(const Invoker&) = delete;
  Invoker& operator=(const Invoker&) = delete;

  // IExperimentalAPIInvoker:
  HRESULT STDMETHODCALLTYPE IsSupported(PCWSTR method_name,
                                        BOOL* is_supported) override {
    if (!method_name || !is_supported) {
      return E_POINTER;
    }
    *is_supported = shell_ ? shell_->IsMethodSupported(method_name)
                           : IsApiMethod(method_name);
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE Invoke(PCWSTR method_name,
                                   INamedPropertyStore* args,
                                   BOOL* is_supported) override {
    if (!method_name || !is_supported) {
      return E_POINTER;
    }
    if (!shell_) {
      *is_supported = IsApiMethod(method_name);
      return RPC_E_DISCONNECTED;
    }
    return shell_->OnInvoke(method_name, args, is_supported);
  }

 private:
  ~Invoker() override = default;

  const base::WeakPtr<FakeDirectLaunchShell> shell_;
};

// The class object registered with COM for the API's CLSID.
class FakeDirectLaunchShell::ClassFactory
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IClassFactory> {
 public:
  explicit ClassFactory(base::WeakPtr<FakeDirectLaunchShell> shell)
      : shell_(std::move(shell)) {}
  ClassFactory(const ClassFactory&) = delete;
  ClassFactory& operator=(const ClassFactory&) = delete;

  // IClassFactory:
  HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer,
                                           REFIID iid,
                                           void** object) override {
    if (!object) {
      return E_POINTER;
    }
    *object = nullptr;
    if (outer) {
      return CLASS_E_NOAGGREGATION;
    }
    if (!shell_) {
      return RPC_E_DISCONNECTED;
    }
    const HRESULT hr = shell_->OnCreate();
    if (FAILED(hr)) {
      return hr;
    }
    return Microsoft::WRL::Make<Invoker>(shell_).CopyTo(iid, object);
  }
  HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override { return S_OK; }

 private:
  ~ClassFactory() override = default;

  const base::WeakPtr<FakeDirectLaunchShell> shell_;
};

FakeDirectLaunchShell::UriRegistration::UriRegistration() = default;
FakeDirectLaunchShell::UriRegistration::UriRegistration(
    const UriRegistration&) = default;
FakeDirectLaunchShell::UriRegistration&
FakeDirectLaunchShell::UriRegistration::operator=(const UriRegistration&) =
    default;
FakeDirectLaunchShell::UriRegistration::~UriRegistration() = default;

FakeDirectLaunchShell::FakeDirectLaunchShell() : create_result_(S_OK) {
  Microsoft::WRL::ComPtr<IClassFactory> factory =
      Microsoft::WRL::Make<ClassFactory>(weak_factory_.GetWeakPtr());
  HRESULT hr = ::CoRegisterClassObject(
      kExperimentalApiInvokerClsid, factory.Get(), CLSCTX_INPROC_SERVER,
      REGCLS_MULTIPLEUSE, &class_object_cookie_);
  CHECK_EQ(hr, S_OK) << logging::SystemErrorCodeToString(hr);

  // Make sure that COM resolves the API to this fake rather than to the shell,
  // which would otherwise be registered for real on recent versions of Windows.
  Microsoft::WRL::ComPtr<IClassFactory> resolved;
  hr = ::CoGetClassObject(kExperimentalApiInvokerClsid, CLSCTX_INPROC_SERVER,
                          nullptr, IID_PPV_ARGS(&resolved));
  CHECK_EQ(hr, S_OK) << logging::SystemErrorCodeToString(hr);
  CHECK_EQ(resolved.Get(), factory.Get());
}

FakeDirectLaunchShell::~FakeDirectLaunchShell() {
  const HRESULT hr = ::CoRevokeClassObject(class_object_cookie_);
  CHECK_EQ(hr, S_OK) << logging::SystemErrorCodeToString(hr);
}

void FakeDirectLaunchShell::SetInvokeResult(std::wstring_view method,
                                            HRESULT result) {
  CHECK(IsApiMethod(method));
  CHECK(result == S_OK || FAILED(result));
  if (result == S_OK) {
    invoke_results_.erase(method);
  } else {
    invoke_results_.insert_or_assign(std::wstring(method), result);
  }
}

void FakeDirectLaunchShell::SetMethodSupported(std::wstring_view method,
                                               bool is_supported) {
  CHECK(IsApiMethod(method));
  if (is_supported) {
    unsupported_methods_.erase(method);
  } else {
    unsupported_methods_.emplace(method);
  }
}

void FakeDirectLaunchShell::LaunchUri(std::wstring_view uri) {
  CHECK(uri_registration_.has_value());
  std::wstring payload(uri);
  COPYDATASTRUCT copy_data = {
      .dwData = uri_registration_->copy_data_format_id,
      .cbData =
          base::checked_cast<DWORD>((payload.size() + 1) * sizeof(wchar_t)),
      .lpData = payload.data(),
  };
  SendToWindow(uri_registration_->target_window, WM_COPYDATA, 0,
               reinterpret_cast<LPARAM>(&copy_data));
}

void FakeDirectLaunchShell::LaunchTile() {
  CHECK(tile_registration_.has_value());
  SendToWindow(tile_registration_->target_window,
               tile_registration_->message_id, 0, 0);
}

bool FakeDirectLaunchShell::IsMethodSupported(std::wstring_view method) const {
  return IsApiMethod(method) && !unsupported_methods_.contains(method);
}

HRESULT FakeDirectLaunchShell::OnCreate() {
  ++create_count_;
  return create_result_;
}

HRESULT FakeDirectLaunchShell::OnInvoke(std::wstring_view method,
                                        INamedPropertyStore* args,
                                        BOOL* is_supported) {
  invoked_methods_.emplace_back(method);
  *is_supported = IsMethodSupported(method);
  if (!*is_supported) {
    return S_OK;
  }
  if (auto it = invoke_results_.find(method); it != invoke_results_.end()) {
    return it->second;
  }
  if (method == kRegisterUriLaunch) {
    UriRegistration registration;
    const HRESULT hr = ReadUriRegistration(args, &registration);
    if (FAILED(hr)) {
      return hr;
    }
    uri_registration_ = std::move(registration);
    return S_OK;
  }
  if (method == kRegisterTileLaunch) {
    TileRegistration registration;
    const HRESULT hr = ReadTileRegistration(args, &registration);
    if (FAILED(hr)) {
      return hr;
    }
    tile_registration_ = registration;
    return S_OK;
  }
  if (method == kUnregisterUriLaunch) {
    uri_registration_.reset();
    return S_OK;
  }
  if (method == kUnregisterTileLaunch) {
    tile_registration_.reset();
    return S_OK;
  }
  // Every method of the API is handled above.
  NOTREACHED();
}

}  // namespace direct_launch
