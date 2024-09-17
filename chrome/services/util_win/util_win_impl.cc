// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/util_win/util_win_impl.h"

#include <objbase.h>

#include <shldisp.h>
#include <wrl/client.h>

#include <string>
#include <string_view>
#include <utility>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_variant.h"
#include "base/win/shortcut.h"
#include "base/win/win_util.h"
#include "chrome/browser/win/conflicts/module_info_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/installer/util/registry_util.h"
#include "chrome/installer/util/taskbar_util.h"
#include "chrome/services/util_win/av_products.h"
#include "chrome/services/util_win/tpm_metrics.h"
#include "content/public/common/content_features.h"
#include "third_party/metrics_proto/system_profile.pb.h"
#include "ui/shell_dialogs/execute_select_file_win.h"

namespace {

bool COMAlreadyInitialized() {
  if (base::FeatureList::IsEnabled(features::kUtilWinProcessUsesUiPump) &&
      base::FeatureList::IsEnabled(
          features::kUtilityWithUiPumpInitializesCom)) {
    base::win::AssertComApartmentType(base::win::ComApartmentType::STA);
    return true;
  }
  return false;
}

// This class checks if the current executable is pinned to the taskbar. It also
// keeps track of the errors that occurs that prevents it from getting a result.
class IsPinnedToTaskbarHelper {
 public:
  IsPinnedToTaskbarHelper() {
    if (!COMAlreadyInitialized()) {
      scoped_com_initializer_.emplace();
    }
  }

  IsPinnedToTaskbarHelper(const IsPinnedToTaskbarHelper&) = delete;
  IsPinnedToTaskbarHelper& operator=(const IsPinnedToTaskbarHelper&) = delete;

  // Returns true if the current executable is pinned to the taskbar.
  bool GetResult();

  bool error_occured() { return error_occured_; }

 private:
  // Returns the shell resource string identified by |resource_id|, or an empty
  // string on error.
  std::wstring LoadShellResourceString(uint32_t resource_id);

  // Returns true if the "Unpin from taskbar" verb is available for |shortcut|,
  // which means that the shortcut is pinned to the taskbar.
  bool ShortcutHasUnpinToTaskbarVerb(const base::FilePath& shortcut);

  // Returns true if the target parameter of the |shortcut| evaluates to
  // |program_compare|.
  bool IsShortcutForProgram(const base::FilePath& shortcut,
                            const installer::ProgramCompare& program_compare);

  // Returns true if one of the shortcuts inside the given `directory` evaluates
  // to `program_compare` and is pinned to the taskbar.
  bool DirectoryContainsPinnedShortcutForProgram(
      const base::FilePath& directory,
      const installer::ProgramCompare& program_compare);

  bool error_occured_ = false;
  std::optional<base::win::ScopedCOMInitializer> scoped_com_initializer_;
};

std::wstring IsPinnedToTaskbarHelper::LoadShellResourceString(
    uint32_t resource_id) {
  base::ScopedNativeLibrary scoped_native_library(::LoadLibraryEx(
      FILE_PATH_LITERAL("shell32.dll"), nullptr,
      LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE));
  if (!scoped_native_library.is_valid())
    return std::wstring();

  const wchar_t* resource_ptr = nullptr;
  int length = ::LoadStringW(scoped_native_library.get(), resource_id,
                             reinterpret_cast<wchar_t*>(&resource_ptr), 0);
  if (!length || !resource_ptr)
    return std::wstring();
  return std::wstring(resource_ptr, length);
}

bool IsPinnedToTaskbarHelper::ShortcutHasUnpinToTaskbarVerb(
    const base::FilePath& shortcut) {
  // Found inside shell32.dll's resources.
  constexpr uint32_t kUnpinFromTaskbarID = 5387;

  std::wstring verb_name(LoadShellResourceString(kUnpinFromTaskbarID));
  if (verb_name.empty()) {
    error_occured_ = true;
    return false;
  }

  Microsoft::WRL::ComPtr<IShellDispatch> shell_dispatch;
  HRESULT hresult =
      ::CoCreateInstance(CLSID_Shell, nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&shell_dispatch));
  if (FAILED(hresult) || !shell_dispatch) {
    error_occured_ = true;
    return false;
  }

  Microsoft::WRL::ComPtr<Folder> folder;
  hresult = shell_dispatch->NameSpace(
      base::win::ScopedVariant(shortcut.DirName().value().c_str()), &folder);
  if (FAILED(hresult) || !folder) {
    error_occured_ = true;
    return false;
  }

  Microsoft::WRL::ComPtr<FolderItem> item;
  hresult = folder->ParseName(
      base::win::ScopedBstr(shortcut.BaseName().value()).Get(), &item);
  if (FAILED(hresult) || !item) {
    error_occured_ = true;
    return false;
  }

  Microsoft::WRL::ComPtr<FolderItemVerbs> verbs;
  hresult = item->Verbs(&verbs);
  if (FAILED(hresult) || !verbs) {
    error_occured_ = true;
    return false;
  }

  long verb_count = 0;
  hresult = verbs->get_Count(&verb_count);
  if (FAILED(hresult)) {
    error_occured_ = true;
    return false;
  }

  long error_count = 0;
  for (long i = 0; i < verb_count; ++i) {
    Microsoft::WRL::ComPtr<FolderItemVerb> verb;
    hresult = verbs->Item(base::win::ScopedVariant(i, VT_I4), &verb);
    if (FAILED(hresult) || !verb) {
      error_count++;
      continue;
    }
    base::win::ScopedBstr name;
    hresult = verb->get_Name(name.Receive());
    if (FAILED(hresult)) {
      error_count++;
      continue;
    }
    if (std::wstring_view(name.Get(), name.Length()) == verb_name) {
      return true;
    }
  }

  if (error_count == verb_count)
    error_occured_ = true;

  return false;
}

bool IsPinnedToTaskbarHelper::IsShortcutForProgram(
    const base::FilePath& shortcut,
    const installer::ProgramCompare& program_compare) {
  base::win::ShortcutProperties shortcut_properties;
  if (!ResolveShortcutProperties(
          shortcut, base::win::ShortcutProperties::PROPERTIES_TARGET,
          &shortcut_properties)) {
    return false;
  }

  return program_compare.EvaluatePath(shortcut_properties.target);
}

bool IsPinnedToTaskbarHelper::DirectoryContainsPinnedShortcutForProgram(
    const base::FilePath& directory,
    const installer::ProgramCompare& program_compare) {
  base::FileEnumerator shortcut_enum(directory, false,
                                     base::FileEnumerator::FILES);
  for (base::FilePath shortcut = shortcut_enum.Next(); !shortcut.empty();
       shortcut = shortcut_enum.Next()) {
    if (IsShortcutForProgram(shortcut, program_compare)) {
      std::optional<bool> is_pinned = IsShortcutPinnedToTaskbar(shortcut);
      if (is_pinned == true)
        return true;
      // Fall back to checking for the taskbar verb on versions of Windows that
      // don't support IsShortcutPinnedToTaskbar.
      if (!is_pinned.has_value() && ShortcutHasUnpinToTaskbarVerb(shortcut))
        return true;
    }
  }
  return false;
}

bool IsPinnedToTaskbarHelper::GetResult() {
  base::FilePath current_exe;
  if (!base::PathService::Get(base::FILE_EXE, &current_exe))
    return false;

  installer::ProgramCompare current_exe_compare(current_exe);
  // Look into the "Quick Launch\User Pinned\TaskBar" folder.
  base::FilePath taskbar_pins_dir;
  if (base::PathService::Get(base::DIR_TASKBAR_PINS, &taskbar_pins_dir) &&
      DirectoryContainsPinnedShortcutForProgram(taskbar_pins_dir,
                                                current_exe_compare)) {
    return true;
  }

  // Check all folders in ImplicitAppShortcuts.
  base::FilePath implicit_app_shortcuts_dir;
  if (!base::PathService::Get(base::DIR_IMPLICIT_APP_SHORTCUTS,
                              &implicit_app_shortcuts_dir)) {
    return false;
  }
  base::FileEnumerator directory_enum(implicit_app_shortcuts_dir, false,
                                      base::FileEnumerator::DIRECTORIES);
  for (base::FilePath directory = directory_enum.Next(); !directory.empty();
       directory = directory_enum.Next()) {
    if (DirectoryContainsPinnedShortcutForProgram(directory,
                                                  current_exe_compare)) {
      return true;
    }
  }
  return false;
}

}  // namespace

UtilWinImpl::UtilWinImpl(mojo::PendingReceiver<chrome::mojom::UtilWin> receiver)
    : receiver_(this, std::move(receiver)) {}

UtilWinImpl::~UtilWinImpl() = default;

void UtilWinImpl::IsPinnedToTaskbar(IsPinnedToTaskbarCallback callback) {
  IsPinnedToTaskbarHelper helper;
  bool is_pinned_to_taskbar = helper.GetResult();
  std::move(callback).Run(!helper.error_occured(), is_pinned_to_taskbar);
}

void UtilWinImpl::UnpinShortcuts(
    const std::vector<base::FilePath>& shortcut_paths,
    UnpinShortcutsCallback callback) {
  // TODO(crbug.com/348014083): This exists to preserve an old behavior in
  // an experiment control group. Remove after experiment is complete.
  std::optional<base::win::ScopedCOMInitializer> scoped_com_initializer;
  if (!COMAlreadyInitialized()) {
    scoped_com_initializer.emplace();
  }

  for (const auto& shortcut_path : shortcut_paths)
    UnpinShortcutFromTaskbar(shortcut_path);

  std::move(callback).Run();
}

void UtilWinImpl::CreateOrUpdateShortcuts(
    const std::vector<base::FilePath>& shortcut_paths,
    const std::vector<base::win::ShortcutProperties>& properties,
    base::win::ShortcutOperation operation,
    CreateOrUpdateShortcutsCallback callback) {
  // TODO(crbug.com/348014083): This exists to preserve an old behavior in
  // an experiment control group. Remove after experiment is complete.
  std::optional<base::win::ScopedCOMInitializer> scoped_com_initializer;
  if (!COMAlreadyInitialized()) {
    scoped_com_initializer.emplace();
    if (!scoped_com_initializer->Succeeded()) {
      std::move(callback).Run(false);
      return;
    }
  }

  bool ret = true;
  for (size_t i = 0; i < shortcut_paths.size(); ++i) {
    ret &= base::win::CreateOrUpdateShortcutLink(shortcut_paths[i],
                                                 properties[i], operation);
  }
  std::move(callback).Run(ret);
}

void UtilWinImpl::CallExecuteSelectFile(
    ui::SelectFileDialog::Type type,
    uint32_t owner,
    const std::u16string& title,
    const base::FilePath& default_path,
    const std::vector<ui::FileFilterSpec>& filter,
    int32_t file_type_index,
    const std::u16string& default_extension,
    CallExecuteSelectFileCallback callback) {
  // TODO(crbug.com/348014083): This exists to preserve an old behavior in
  // an experiment control group. Remove after experiment is complete.
  std::optional<base::win::ScopedCOMInitializer> scoped_com_initializer;
  if (!COMAlreadyInitialized()) {
    scoped_com_initializer.emplace();
  }

  base::win::EnableHighDPISupport();

  base::ElapsedTimer elapsed_time;

  ui::ExecuteSelectFile(
      type, title, default_path, filter, file_type_index,
      base::UTF16ToWide(default_extension),
      reinterpret_cast<HWND>(base::win::Uint32ToHandle(owner)),
      std::move(callback));

  base::UmaHistogramMediumTimes("Windows.TimeInSelectFileDialog",
                                elapsed_time.Elapsed());
}

void UtilWinImpl::InspectModule(const base::FilePath& module_path,
                                InspectModuleCallback callback) {
  std::move(callback).Run(::InspectModule(module_path));
}

void UtilWinImpl::GetAntiVirusProducts(bool report_full_names,
                                       GetAntiVirusProductsCallback callback) {
  // TODO(crbug.com/348014083): This exists to preserve an old behavior in
  // an experiment control group. Remove after experiment is complete.
  std::optional<base::win::ScopedCOMInitializer> scoped_com_initializer;
  if (!COMAlreadyInitialized()) {
    scoped_com_initializer.emplace();
  }
  std::move(callback).Run(::GetAntiVirusProducts(report_full_names));
}

void UtilWinImpl::GetTpmIdentifier(bool report_full_names,
                                   GetTpmIdentifierCallback callback) {
  std::move(callback).Run(::GetTpmIdentifier(report_full_names));
}
