// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_IME_IME_SHARED_LIBRARY_WRAPPER_H_
#define CHROMEOS_ASH_SERVICES_IME_IME_SHARED_LIBRARY_WRAPPER_H_

#include <optional>

#include "base/no_destructor.h"
#include "base/scoped_native_library.h"
#include "chromeos/ash/services/ime/public/cpp/shared_lib/interfaces.h"

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

inline constexpr char kInitUserDataServiceFnName[] = "InitUserDataService";
typedef void (*InitUserDataServiceFn)(ImeCrosPlatform* platform);

inline constexpr char kProcessUserDataRequestFnName[] =
    "ProcessUserDataRequest";
typedef C_SerializedProto (*ProcessUserDataRequestFn)(C_SerializedProto args);

inline constexpr char kDeleteSerializedProtoFnName[] = "DeleteSerializedProto";
typedef void (*DeleteSerializedProtoFn)(C_SerializedProto args);

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
    ImeDecoderSupportsFn proto_mode_supports;
    ImeDecoderActivateImeFn proto_mode_activate_ime;
    ImeDecoderProcessFn proto_mode_process;

    InitMojoModeFn init_mojo_mode;
    CloseMojoModeFn close_mojo_mode;
    InitializeConnectionFactoryFn mojo_mode_initialize_connection_factory;
    IsInputMethodConnectedFn mojo_mode_is_input_method_connected;

    InitUserDataServiceFn init_user_data_service;
    ProcessUserDataRequestFn process_user_data_request;
    DeleteSerializedProtoFn delete_serialized_proto;
  };

  // Loads the IME shared library (if not already loaded) then returns its entry
  // points. Entry points are only available if the IME shared library has been
  // successfully loaded.
  virtual std::optional<EntryPoints> MaybeLoadThenReturnEntryPoints() = 0;
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

  std::optional<EntryPoints> MaybeLoadThenReturnEntryPoints() override;

 private:
  friend class base::NoDestructor<ImeSharedLibraryWrapperImpl>;

  explicit ImeSharedLibraryWrapperImpl();
  ~ImeSharedLibraryWrapperImpl() override;

  // Result of IME decoder DSO initialization.
  std::optional<base::ScopedNativeLibrary> library_;

  std::optional<EntryPoints> entry_points_;
};

}  // namespace ime
}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_IME_IME_SHARED_LIBRARY_WRAPPER_H_
