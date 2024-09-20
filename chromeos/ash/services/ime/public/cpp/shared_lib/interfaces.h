// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_SHARED_LIB_INTERFACES_H_
#define CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_SHARED_LIB_INTERFACES_H_

#include <stddef.h>
#include <stdint.h>

// Introduce interfaces between the IME Mojo service and its IME shared library
// which is loaded dynamically during runtime.
//
// Prerequisite: Compilers of both IME service and IME shared library should
// follow the same or similar ABIs, so that their generated code should be
// interoperable.
//
// Please note that the IME shared library is not likely compiled with the same
// compiler as the IME Mojo service. IME shared library's creater should be
// responsible for compiling it in a compiler with the same or similar ABIs.
//
// For polymorphism, the interfaces should be abstract base classes with pure
// virtual methods. An interface will be implemented in one end and called on
// the other end.
//
//                          +-----------+
//                    .---> | Interface | <---.
//         depends on |     +-----------+     | depends on
//                    |                       |
//           +----------------+         +-------------+
//           | Shared Library |         | IME Service |
//           +----------------+         +-------------+
//
// For best practices on creating/modifying these interfaces.
//
// 1, Stick with "C" style arguments and return types over these interfaces,
// because the "C" ABIs are much more stable. E.g. std structures over these
// interfaces is not likely layout-compatible between compilers.
//
// 2, Every interface here should only contain pure virtual methods, except
// its destructor.
//
// 3, Protect destructors of these interfaces to discourage clients from
// deleting interface. Providing a Destroy function if needed.
//
// 4, Always add new methods at the end of an interface, and do not add new
// virtual overloaded functions (methods with the same name).
//
// 5, Document the ownership of these interfaces' parameters to avoid memory
// leaks or overflows.
//
// IME service and its IME shared library need to be recompiled when these
// interfaces change. Keep these interfaces as stable as possible, so making
// changes to the implementation of an interface will not need to recompile the
// caller end.
//
// And it's important to keep any unnecessary information out of this header.

// Forward declare MojoSystemThunks to keep this file free of direct
// dependencies on Chromium, making it easier to use this file outside of the
// Chromium repo. When using this file, consumers should also #include their own
// copy of the MojoSystemThunks struct definition.
struct MojoSystemThunks2;

namespace ash {
namespace ime {

enum SimpleDownloadStatusCode {
  // The download succeeded.
  SIMPLE_DOWNLOAD_STATUS_OK = 0,
  // The download failed due to an invalid url or path.
  SIMPLE_DOWNLOAD_STATUS_INVALID_ARGUMENT = -1,
  // The download failed because Chrome disconnected from IME Service.
  SIMPLE_DOWNLOAD_STATUS_ABORTED = -2,
};

// A simple downloading callback with the downloading URL as return.
typedef void (*SimpleDownloadCallbackV2)(SimpleDownloadStatusCode status_code,
                                         const char* url,
                                         const char* file_path);

// A function pointer of a sequenced task.
typedef void (*ImeSequencedTask)(int task_id);

// A logger function pointer from chrome.
typedef void (*ChromeLoggerFunc)(int severity, const char* message);

// ============================================================================
// [Proto + Mojo modes] [IME shared lib --> IME service container]
// ============================================================================
// Used by the IME shared lib to access platform-specific data and operations.
// Provided by the IME service container upon invoking ImeDecoderInitOnce "C"
// API entry point of IME shared lib. Always owned by the IME service container.
// ============================================================================
class ImeCrosPlatform {
 protected:
  virtual ~ImeCrosPlatform() = default;

 public:
  // Get the read-only local IME bundle directory. IME service could be running
  // in a mode where the directory is unavailable, in which case this will
  // return empty. Returned pointer remains valid until `Platform` is destroyed.
  virtual const char* GetImeBundleDir() = 0;

  // Obsolete, thus deprecated and must not be used. Kept for ABI vtable compat.
  virtual void Unused3() = 0;

  // Get the local IME directory in home directory of the active user, which is
  // only accessible to the user itself. IME service could be running in a mode
  // where the directory is unavailable, in which case this will return empty.
  // Returned pointer remains valid until `Platform` is destroyed.
  virtual const char* GetImeUserHomeDir() = 0;

  // Obsolete, thus deprecated and must not be used. Kept for ABI vtable compat.
  virtual void Unused1() = 0;

  // Obsolete, thus deprecated and must not be used. Kept for ABI vtable compat.
  virtual void Unused2() = 0;

  // This is used for decoder to run some Mojo-specific operation which is
  // required to run in the thread creating its remote.
  virtual void RunInMainSequence(ImeSequencedTask task, int task_id) = 0;

  // Returns whether a CrOS experimental feature is enabled. Only a subset of
  // CrOS features are considered (features not considered appear as disabled).
  // |feature_name| corresponds to base::Feature::name of the CrOS feature as
  // defined in ash/constants/ash_features.cc in the Chromium repo.
  virtual bool IsFeatureEnabled(const char* feature_name) = 0;

  // Start a download using |SimpleURLLoader|. Each SimpleDownloadToFileV2 can
  // only be used for a single request. Make a call after the previous task
  // completes or cancels. There's download URL included in the callback.
  virtual int SimpleDownloadToFileV2(const char* url,
                                     const char* file_path,
                                     SimpleDownloadCallbackV2 callback) = 0;

  virtual const void* Unused4() = 0;

  // Retrieves the string value of a CrOS feature's Finch param. Only a subset
  // of CrOS features are considered (see impl for details). |feature_name| is
  // defined in base::Feature::name for each CrOS feature. If the feature isn't
  // enabled or isn't considered, or the param doesn't exist, returns an empty
  // string. Ownership of the returned string is transferred to the caller who
  // should be in charge of releasing its memory when it's no longer in use.
  virtual const char* GetFieldTrialParamValueByFeature(
      const char* feature_name,
      const char* param_name) = 0;

  // Returns a pointer to the Mojo system thunks.
  // The shared library can use this pointer for its own Mojo environment in
  // order to communicate directly with the browser process.
  // MojoSystemThunks has a stable ABI, hence it is safe to use it from the
  // shared library
  virtual const MojoSystemThunks2* GetMojoSystemThunks2() = 0;

  // TODO(https://crbug.com/837156): Provide Logger for main entry.
};

// ============================================================================
// [Proto mode only] [IME shared lib --> IME service container]
// ============================================================================
// Used to send messages to connected IME client from an IME engine. IME service
// container will create an instance then pass it to the IME shared lib via
// Proto-mode ImeDecoderActivateIme "C" API entry point.
// ============================================================================
class ImeClientDelegate {
 protected:
  virtual ~ImeClientDelegate() = default;

 public:
  // Obsolete, thus deprecated and must not be used. Kept for ABI vtable compat.
  virtual void Unused1() = 0;

  // Process response data from the IME shared lib in the connected IME client.
  // The data will be invalidated by the engine soon after this call.
  virtual void Process(const uint8_t* data, size_t size) = 0;

  // Destroy the `ImeClientDelegate` instance, which is called in the shared
  // library when the bound engine is destroyed.
  virtual void Destroy() = 0;
};

}  // namespace ime
}  // namespace ash

// ============================================================================
// [Proto + Mojo modes] [IME service container --> IME shared lib]
// ============================================================================
// "C" API exposed by the "CrOS 1P IME shared lib", to be dynamically looked up
// and invoked via reflection by "CrOS IME Service container" in Chrome-on-CrOS
// ============================================================================
//
// The IME shared lib should be in either "Proto mode" (default) or "Mojo mode".
// Most "C" API entry points are associated with one particular mode each. When
// such an entry point is invoked, the runtime should switch to its mode if not
// already in that mode; the other mode's state is destroyed upon switching.
//
// - Proto mode: IME service container and IME shared lib talk to each other by
// serialised protobufs sent via the "C" API specified here. Used for VK and
// extension-based PK hosted in the VK+IME extension.
//
// - Mojo mode: IME service container bootstraps Mojo connection with IME shared
// lib via the "C" API specified here. The connection is then used for CrOS IMF
// to talk directly with IME shared lib. Used for non-extension System PK.
//
extern "C" {

// ****************************************************************************
// ************************** (mode agnostic) *********************************
// ****************************************************************************

// Sets logger for the shared library. Releases the previous logger if there
// was one. If the new logger is null, then no logger will be used.
__attribute__((visibility("default"))) void SetImeEngineLogger(
    ash::ime::ChromeLoggerFunc logger_func);

// ****************************************************************************
// ***************************** PROTO MODE ***********************************
// ****************************************************************************

// Initialises the IME shared lib's Proto mode. In Proto mode, client must
// call this function before any other Proto-mode functions. `platform` must
// remain valid during the whole life of the IME shared lib.
__attribute__((visibility("default"))) void InitProtoMode(
    ash::ime::ImeCrosPlatform* platform);

// Closes the IME shared lib's Proto mode and releases resources used by it.
__attribute__((visibility("default"))) void CloseProtoMode();

// Returns whether an IME is supported by this IME shared lib. `ime_spec` is
// the IME's specification name; caller should know its naming rules.
__attribute__((visibility("default"))) bool ImeDecoderSupports(
    const char* ime_spec);

// Activates an IME in the IME shared lib, with a bound `delegate` for callback
// to the client. Ownership of `delegate` is passed to the IME instance.
// TODO(googleo): Remove this and pass `delegate` upon ImeDecoderInitOnce.
__attribute__((visibility("default"))) bool ImeDecoderActivateIme(
    const char* ime_spec,
    ash::ime::ImeClientDelegate* delegate);

// Processes IME events sent from client in serialised protobuf `data` which
// should be invalidated by this IME shared lib soon after it's consumed.
__attribute__((visibility("default"))) void ImeDecoderProcess(
    const uint8_t* data,
    size_t size);

// ****************************************************************************
// ***************************** MOJO MODE ************************************
// ****************************************************************************

// Initialises the IME shared lib's Mojo mode. In Mojo mode, client must
// call this function before any other Mojo-mode functions. `platform` must
// remain valid during the whole life of the IME shared lib.
__attribute__((visibility("default"))) void InitMojoMode(
    ash::ime::ImeCrosPlatform* platform);

// Closes the IME shared lib's Mojo mode and releases resources used by it.
__attribute__((visibility("default"))) void CloseMojoMode();

// Bootstraps an implementation of a ConnectionFactory in the IME shared lib.
// Returns false if the connection attempt was unsuccessful.
__attribute__((visibility("default"))) bool InitializeConnectionFactory(
    uint32_t receiver_connection_factory_handle);

// Returns whether there's a direct Mojo connection to an input method.
__attribute__((visibility("default"))) bool IsInputMethodConnected();

// ****************************************************************************
// ***************************** USER DATA ************************************
// ****************************************************************************

// Contains serialised proto data with the size of the buffer.
struct C_SerializedProto {
  const uint8_t* const buffer;
  const size_t size;
};

// If the proto is created in the shared library side, this function will clear
// up the memory from the shared library side to prevent unintended issues due
// to mismatch in memory management implementation.
__attribute__((visibility("default"))) void DeleteSerializedProto(
    C_SerializedProto proto);

// Initialises the IME shared lib's UserData service functionality.
// This needs to be called before any "UserData*" functions are called.
__attribute__((visibility("default"))) void InitUserDataService(
    ash::ime::ImeCrosPlatform* platform);

// args: serialized UserDataRequest proto.
// returns serialized UserDataResponse proto.
// (see ./proto/user_data_service.proto)
__attribute__((visibility("default"))) C_SerializedProto ProcessUserDataRequest(
    C_SerializedProto args);

}  // extern "C"

#endif  // CHROMEOS_ASH_SERVICES_IME_PUBLIC_CPP_SHARED_LIB_INTERFACES_H_
