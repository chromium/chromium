// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_IME_IME_SHARED_LIBRARY_WRAPPER_H_
#define CHROMEOS_ASH_SERVICES_IME_IME_SHARED_LIBRARY_WRAPPER_H_

#include "chromeos/ash/services/ime/public/cpp/shared_lib/interfaces.h"

#include "base/no_destructor.h"
#include "base/scoped_native_library.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace ime {

// START: Signatures of "C" API entry points of CrOS 1P IME shared library.
// Must match API specs in
// chromeos/ash/services/ime/public/cpp/shared_lib/interfaces.h

inline constexpr char kSetImeEngineLoggerFnName[] = "SetImeEngineLogger";
typedef void (*SetImeEngineLoggerFn)(ChromeLoggerFunc logger_func);

inline constexpr char kInitProtoModeFnName[] = "InitProtoMode";
typedef void (*InitProtoModeFn)(ImeCrosPlatform* platform);

inline constexpr char kCloseProtoModeFnName[] = "CloseProtoMode";
typedef void (*CloseProtoModeFn)();

inline constexpr char kImeDecoderSupportsFnName[] = "ImeDecoderSupports";
typedef bool (*ImeDecoderSupportsFn)(const char* ime_spec);

inline constexpr char kImeDecoderActivateImeFnName[] = "ImeDecoderActivateIme";
typedef bool (*ImeDecoderActivateImeFn)(const char* ime_spec,
                                        ImeClientDelegate* delegate);

inline constexpr char kImeDecoderProcessFnName[] = "ImeDecoderProcess";
typedef void (*ImeDecoderProcessFn)(const uint8_t* data, size_t size);

inline constexpr char kInitMojoModeFnName[] = "InitMojoMode";
typedef void (*InitMojoModeFn)(ImeCrosPlatform* platform);

inline constexpr char kCloseMojoModeFnName[] = "CloseMojoMode";
typedef void (*CloseMojoModeFn)();

inline constexpr char kInitializeConnectionFactoryFnName[] =
    "InitializeConnectionFactory";
typedef bool (*InitializeConnectionFactoryFn)(
    uint32_t receiver_connection_factory_handle);

inline constexpr char kIsInputMethodConnectedFnName[] =
    "IsInputMethodConnected";
typedef bool (*IsInputMethodConnectedFn)();

// END: Signatures of "C" API entry points of CrOS 1P IME shared lib.

// This class manages the dynamic loading of CrOS 1P IME shared lib
// .so, and facilitates access to its "C" API entry points.
class ImeSharedLibraryWrapper {
 public:
  virtual ~ImeSharedLibraryWrapper() = default;

  // Function pointers to "C" API entry points of the loaded IME shared library.
  // See chromeos/ash/services/ime/public/cpp/shared_lib/interfaces.h for API
  // specs.
  struct EntryPoints {
    InitProtoModeFn init_proto_mode;
    CloseProtoModeFn close_proto_mode;

    // TODO(b/214153032): Prefix the following with "proto_mode_" to better
    // indicate they only pertain to the IME shared lib's ProtoMode. While it's
    // "hard" to rename corresponding "C" API functions due to cross-repo
    // backward compat requirements, these are local and rename is feasible.
    ImeDecoderSupportsFn supports;
    ImeDecoderActivateImeFn activate_ime;
    ImeDecoderProcessFn process;

    InitMojoModeFn init_mojo_mode;
    CloseMojoModeFn close_mojo_mode;
    InitializeConnectionFactoryFn initialize_connection_factory;
    IsInputMethodConnectedFn is_input_method_connected;
  };

  // Loads the IME shared library (if not already loaded) then returns its entry
  // points. Entry points are only available if the IME shared library has been
  // successfully loaded.
  virtual absl::optional<EntryPoints> MaybeLoadThenReturnEntryPoints() = 0;
};

// A proxy class for the IME decoder.
// ImeSharedLibraryWrapper is implemented as a singleton and is initialized
// before 'ime' sandbox is engaged.
class ImeSharedLibraryWrapperImpl : public ImeSharedLibraryWrapper {
 public:
  // Gets the singleton ImeDecoderImpl.
  static ImeSharedLibraryWrapperImpl* GetInstance();

  ImeSharedLibraryWrapperImpl(const ImeSharedLibraryWrapperImpl&) = delete;
  ImeSharedLibraryWrapperImpl& operator=(const ImeSharedLibraryWrapperImpl&) =
      delete;

  absl::optional<EntryPoints> MaybeLoadThenReturnEntryPoints() override;

 private:
  friend class base::NoDestructor<ImeSharedLibraryWrapperImpl>;

  explicit ImeSharedLibraryWrapperImpl();
  ~ImeSharedLibraryWrapperImpl() override;

  // Result of IME decoder DSO initialization.
  absl::optional<base::ScopedNativeLibrary> library_;

  absl::optional<EntryPoints> entry_points_;
};

}  // namespace ime
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_IME_IME_SHARED_LIBRARY_WRAPPER_H_
