// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/enumerate_shell_extensions.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/win/registry.h"
#include "chrome/browser/win/conflicts/module_info_util.h"

// The following link explains how Shell extensions are registered on Windows.
// Different types of handlers can be applied to different types of Shell
// objects.
// https://docs.microsoft.com/en-us/windows/desktop/shell/reg-shell-exts

namespace {

// The different kinds of shell objects that can be affected by a shell
// extension.
constexpr wchar_t kEverything[] = L"*";
constexpr wchar_t kAllFileSystemObjects[] = L"AllFileSystemObjects";
constexpr wchar_t kDesktopBackground[] = L"DesktopBackground";
constexpr wchar_t kDirectory[] = L"Directory";
constexpr wchar_t kDirectoryBackground[] = L"Directory\\Background";
constexpr wchar_t kDrive[] = L"Drive";
constexpr wchar_t kFolder[] = L"Folder";
constexpr wchar_t kNetServer[] = L"NetServer";
constexpr wchar_t kNetShare[] = L"NetShare";
constexpr wchar_t kNetwork[] = L"Network";
constexpr wchar_t kPrinters[] = L"Printers";

// Retrieves the path to the registry key that contains all the shell extensions
// of type |shell_extension_type| that apply to |shell_object_type|.
base::string16 GetShellExtensionTypePath(const wchar_t* shell_extension_type,
                                         const wchar_t* shell_object_type) {
  return base::StringPrintf(L"%ls\\shellex\\%ls", shell_object_type,
                            shell_extension_type);
}

// Returns the path to the DLL for an InProcServer32 registration.
base::FilePath GetInProcServerPath(const wchar_t* guid) {
  base::string16 key = base::StringPrintf(kClassIdRegistryKeyFormat, guid);

  base::win::RegKey clsid;
  if (clsid.Open(HKEY_CLASSES_ROOT, key.c_str(), KEY_QUERY_VALUE) !=
      ERROR_SUCCESS) {
    return base::FilePath();
  }

  base::string16 dll_path;
  if (clsid.ReadValue(L"", &dll_path) != ERROR_SUCCESS)
    return base::FilePath();

  return base::FilePath(std::move(dll_path));
}

// Reads all the shell extensions of type |shell_extension_type| that are
// applied to |shell_object_types| and forwards them to |callback|.
void ReadShellExtensions(
    const wchar_t* shell_extension_type,
    const wchar_t* shell_object_type,
    const base::RepeatingCallback<void(const base::FilePath&)>& callback) {
  base::string16 path =
      GetShellExtensionTypePath(shell_extension_type, shell_object_type);

  DCHECK_NE(path.back(), L'\\');

  base::string16 guid;
  for (base::win::RegistryKeyIterator iter(HKEY_CLASSES_ROOT, path.c_str());
       iter.Valid(); ++iter) {
    base::string16 shell_extension_reg_path = path + L"\\" + iter.Name();
    base::win::RegKey reg_key(
        HKEY_CLASSES_ROOT, shell_extension_reg_path.c_str(), KEY_QUERY_VALUE);
    if (!reg_key.Valid())
      continue;

    guid.clear();
    reg_key.ReadValue(nullptr, &guid);
    if (guid.empty())
      continue;

    base::FilePath shell_extension_path = GetInProcServerPath(guid.c_str());
    if (shell_extension_path.empty())
      continue;

    callback.Run(shell_extension_path);
  }
}

// Reads all the shell extensions that are in the Approved list and forwards
// them to |callback|.
void ReadApprovedShellExtensions(
    HKEY parent,
    const base::RepeatingCallback<void(const base::FilePath&)>& callback) {
  for (base::win::RegistryValueIterator iter(
           parent, kApprovedShellExtensionRegistryKey);
       iter.Valid(); ++iter) {
    // Skip the key's default value.
    if (!*iter.Name())
      continue;

    base::FilePath shell_extension_path = GetInProcServerPath(iter.Name());
    if (shell_extension_path.empty())
      continue;

    callback.Run(shell_extension_path);
  }
}

// Reads all the shell extensions of type ColumnHandlers.
void ReadColumnHandlers(
    const base::RepeatingCallback<void(const base::FilePath&)>& callback) {
  // Column handlers can only be applied to folders.
  ReadShellExtensions(L"ColumnHandlers", kFolder, callback);
}

// Reads all the shell extensions of type CopyHookHandlers.
void ReadCopyHookHandlers(
    const base::RepeatingCallback<void(const base::FilePath&)>& callback) {
  static constexpr const wchar_t* kSupportedShellObjects[] = {
      kDirectory,
      kPrinters,
  };

  for (const auto* shell_object : kSupportedShellObjects)
    ReadShellExtensions(L"CopyHookHandlers", shell_object, callback);
}

// Reads all the shell extensions of type DragDropHandlers.
void ReadDragDropHandlers(
    const base::RepeatingCallback<void(const base::FilePath&)>& callback) {
  static constexpr const wchar_t* kSupportedShellObjects[] = {
      kDirectory,
      kDrive,
      kFolder,
  };

  for (const auto* shell_object : kSupportedShellObjects)
    ReadShellExtensions(L"DragDropHandlers", shell_object, callback);
}

// Reads all the shell extensions of type ContextMenuHandlers and
// PropertySheetHandlers.
void ReadContextMenuAndPropertySheetHandlers(
    const base::RepeatingCallback<void(const base::FilePath&)>& callback) {
  static constexpr const wchar_t* kHandlerTypes[] = {
      L"ContextMenuHandlers",
      L"PropertySheetHandlers",
  };

  // This list is not exhaustive and does not cover cases where a shell
  // extension is installed for a specific file extension or a specific
  // executable. It should still pick up all the general-purpose shell
  // extensions.
  static constexpr const wchar_t* kSupportedShellObjects[] = {
      kEverything,
      kAllFileSystemObjects,
      kFolder,
      kDirectory,
      kDirectoryBackground,
      kDesktopBackground,
      kDrive,
      kNetwork,
      kNetShare,
      kNetServer,
      kPrinters,
  };

  for (const auto* handler_type : kHandlerTypes) {
    for (const auto* shell_object : kSupportedShellObjects)
      ReadShellExtensions(handler_type, shell_object, callback);
  }
}

// Retrieves the module size and time date stamp for the shell extension and
// forwards it to the callback on |task_runner|.
void OnShellExtensionPathEnumerated(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    OnShellExtensionEnumeratedCallback on_shell_extension_enumerated,
    const base::FilePath& path) {
  uint32_t size_of_image = 0;
  uint32_t time_date_stamp = 0;
  if (!GetModuleImageSizeAndTimeDateStamp(path, &size_of_image,
                                          &time_date_stamp)) {
    return;
  }

  task_runner->PostTask(
      FROM_HERE, base::BindRepeating(std::move(on_shell_extension_enumerated),
                                     path, size_of_image, time_date_stamp));
}

void EnumerateShellExtensionsOnBlockingSequence(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    OnShellExtensionEnumeratedCallback on_shell_extension_enumerated,
    base::OnceClosure on_enumeration_finished) {
  internal::EnumerateShellExtensionPaths(
      base::BindRepeating(&OnShellExtensionPathEnumerated, task_runner,
                          std::move(on_shell_extension_enumerated)));

  task_runner->PostTask(FROM_HERE, std::move(on_enumeration_finished));
}

}  // namespace

const wchar_t kApprovedShellExtensionRegistryKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved";

void EnumerateShellExtensions(
    OnShellExtensionEnumeratedCallback on_shell_extension_enumerated,
    base::OnceClosure on_enumeration_finished) {
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&EnumerateShellExtensionsOnBlockingSequence,
                     base::SequencedTaskRunnerHandle::Get(),
                     std::move(on_shell_extension_enumerated),
                     std::move(on_enumeration_finished)));
}

namespace internal {

void EnumerateShellExtensionPaths(
    const base::RepeatingCallback<void(const base::FilePath&)>& callback) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  ReadApprovedShellExtensions(HKEY_LOCAL_MACHINE, callback);
  ReadApprovedShellExtensions(HKEY_CURRENT_USER, callback);
  ReadColumnHandlers(callback);
  ReadCopyHookHandlers(callback);
  ReadDragDropHandlers(callback);
  ReadContextMenuAndPropertySheetHandlers(callback);
}

}  // namespace internal
