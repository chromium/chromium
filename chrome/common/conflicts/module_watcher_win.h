// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CONFLICTS_MODULE_WATCHER_WIN_H_
#define CHROME_COMMON_CONFLICTS_MODULE_WATCHER_WIN_H_

#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"

class ModuleWatcherTest;

union LDR_DLL_NOTIFICATION_DATA;

// This class observes modules as they are loaded into a process's address
// space.
//
// This class is safe to be created on any thread. Similarly, it is safe to be
// destroyed on any thread, independent of the thread on which the instance was
// created.
class ModuleWatcher {
 public:
  // The types of module events that can occur.
  enum class ModuleEventType {
    // A module was already loaded, but its presence is being observed.
    kModuleAlreadyLoaded,
    // A module is in the process of being loaded.
    kModuleLoaded,
  };

  // Houses information about a module event, and some module metadata.
  struct ModuleEvent {
    ModuleEvent() = default;
    ModuleEvent(const ModuleEvent& other) = default;
    ModuleEvent(ModuleEventType event_type,
                const base::FilePath& module_path,
                void* module_load_address,
                size_t module_size)
        : event_type(event_type),
          module_path(module_path),
          module_load_address(module_load_address),
          module_size(module_size) {}

    // The type of module event.
    ModuleEventType event_type;
    // The full path to the module on disk.
    base::FilePath module_path;
    // The load address of the module. Careful consideration must be made before
    // accessing memory at this address. See the comment for
    // OnModuleEventCallback.
    void* module_load_address;
    // The size of the module in memory.
    size_t module_size;
  };

  // The type of callback that will be invoked for each module event. This
  // callback may be run from any thread in the process, and may be invoked
  // during initialization (while iterating over already loaded modules) or in
  // response to LdrDllNotifications received from the loader. As such, keep the
  // amount of work performed here to an absolute minimum.
  //
  // MODULE_LOADED events are always dispatched directly from the loader while
  // under the loader's lock, so the module is guaranteed to be loaded in memory
  // (it is safe to access module_load_address).
  //
  // If the event is of type MODULE_ALREADY_LOADED, then the module data comes
  // from a snapshot and it is possible that its |module_load_address| is
  // invalid by the time the event is sent.
  //
  // Note that it is possible for this callback to be invoked after the
  // destruction of the watcher.
  using OnModuleEventCallback =
      base::RepeatingCallback<void(const ModuleEvent& event)>;

  // Creates and starts a watcher. This enumerates all loaded modules
  // synchronously on the current thread during construction, and provides
  // synchronous notifications as modules are loaded. The callback is invoked in
  // the context of the thread that is loading a module, and as such may be
  // invoked on any thread in the process. Note that it is possible to receive
  // two notifications for some modules as the initial loaded module enumeration
  // races briefly with the callback mechanism. In this case both a
  // MODULE_LOADED and a MODULE_ALREADY_LOADED event will be received for the
  // same module. Since the callback is installed first no modules can be
  // missed, however. This factory function may be called on any thread.
  //
  // Only a single instance of a watcher may exist at any moment. This will
  // return nullptr when trying to create a second watcher.
  static std::unique_ptr<ModuleWatcher> Create(OnModuleEventCallback callback);

  // This can be called on any thread. After destruction the |callback|
  // provided to the constructor will no longer be invoked with module events.
  ~ModuleWatcher();

 private:
  // For unittesting.
  friend class ModuleWatcherTest;

  // Private to enforce Singleton semantics. See Create above.
  ModuleWatcher();

  // Initializes the ModuleWatcher instance.
  void Initialize(OnModuleEventCallback callback);

  // Registers a DllNotification callback with the OS. Modifies
  // |dll_notification_cookie_|. Can be called on any thread.
  void RegisterDllNotificationCallback();

  // Removes the installed DllNotification callback. Modifies
  // |dll_notification_cookie_|. Can be called on any thread.
  void UnregisterDllNotificationCallback();

  // Enumerates all currently loaded modules, synchronously invoking callbacks
  // on the current thread. Can be called on any thread.
  static void EnumerateAlreadyLoadedModules(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      OnModuleEventCallback callback);

  // Helper function for retrieving the callback associated with a given
  // LdrNotification context.
  static OnModuleEventCallback GetCallbackForContext(void* context);

  // The loader notification callback. This is actually
  // void CALLBACK LoaderNotificationCallback(
  //     DWORD, const LDR_DLL_NOTIFICATION_DATA*, PVOID)
  // Not using CALLBACK/DWORD/PVOID allows skipping the windows.h header from
  // this file.
  static void __stdcall LoaderNotificationCallback(
      unsigned long notification_reason,
      const LDR_DLL_NOTIFICATION_DATA* notification_data,
      void* context);

  // Used to bind the |callback_| to a WeakPtr.
  void RunCallback(const ModuleEvent& event);

  // The current callback. Can end up being invoked on any thread.
  OnModuleEventCallback callback_;
  // Used by the DllNotification mechanism.
  void* dll_notification_cookie_ = nullptr;

  base::WeakPtrFactory<ModuleWatcher> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ModuleWatcher);
};

#endif  // CHROME_COMMON_CONFLICTS_MODULE_WATCHER_WIN_H_
