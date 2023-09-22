// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/conflicts/enumerate_input_method_editors.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/win/registry.h"
#include "chrome/browser/win/conflicts/module_info_util.h"

namespace {

// Returns true if |ime_guid| is the GUID of a built-in Microsoft IME.
bool IsMicrosoftIme(const wchar_t* ime_guid) {
  // This list was provided by Microsoft.
  static constexpr const wchar_t* kMicrosoftImeGuids[] = {
      L"{0000897b-83df-4b96-be07-0fb58b01c4a4}",
      L"{03b5835f-f03c-411b-9ce2-aa23e1171e36}",
      L"{07eb03d6-b001-41df-9192-bf9b841ee71f}",
      L"{3697c5fa-60dd-4b56-92d4-74a569205c16}",
      L"{531fdebf-9b4c-4a43-a2aa-960e8fcdc732}",
      L"{6a498709-e00b-4c45-a018-8f9e4081ae40}",
      L"{78cb5b0e-26ed-4fcc-854c-77e8f3d1aa80}",
      L"{81d4e9c9-1d3b-41bc-9e6c-4b40bf79e35e}",
      L"{8613e14c-d0c0-4161-ac0f-1dd2563286bc}",
      L"{a028ae76-01b1-46c2-99c4-acd9858ae02f}",
      L"{a1e2b86b-924a-4d43-80f6-8a820df7190f}",
      L"{ae6be008-07fb-400d-8beb-337a64f7051f}",
      L"{b115690a-ea02-48d5-a231-e3578d2fdf80}",
      L"{c1ee01f2-b3b6-4a6a-9ddd-e988c088ec82}",
      L"{dcbd6fa8-032f-11d3-b5b1-00c04fc324a1}",
      L"{e429b25a-e5d3-4d1f-9be3-0c608477e3a1}",
      L"{f25e9f57-2fc8-4eb3-a41a-cce5f08541e6}",
      L"{f89e9e58-bd2f-4008-9ac2-0f816c09f4ee}",
      L"{fa445657-9379-11d6-b41a-00065b83ee53}",
  };

  auto comp = [](const wchar_t* lhs, const wchar_t* rhs) -> bool {
    return base::CompareCaseInsensitiveASCII(lhs, rhs) == -1;
  };

  DCHECK(std::is_sorted(std::begin(kMicrosoftImeGuids),
                        std::end(kMicrosoftImeGuids), comp));

  return std::binary_search(std::begin(kMicrosoftImeGuids),
                            std::end(kMicrosoftImeGuids), ime_guid, comp);
}

// Returns the path to the in-proc server DLL for |guid|, or an empty path if
// none is found.
base::FilePath GetInprocServerDllPath(const wchar_t* guid) {
  const std::wstring key_name = GuidToClsid(guid);
  base::win::RegKey registry_key;
  std::wstring value;
  if (registry_key.Open(HKEY_CLASSES_ROOT, key_name.c_str(), KEY_QUERY_VALUE) ==
          ERROR_SUCCESS &&
      registry_key.ReadValue(L"", &value) == ERROR_SUCCESS) {
    return base::FilePath(value);
  }

  return base::FilePath();
}

void EnumerateImesOnBlockingSequence(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    OnImeEnumeratedCallback on_ime_enumerated,
    base::OnceClosure on_enumeration_finished) {
  int nb_imes = 0;
  for (base::win::RegistryKeyIterator iter(HKEY_LOCAL_MACHINE, kImeRegistryKey);
       iter.Valid(); ++iter) {
    const wchar_t* guid = iter.Name();

    // Skip Microsoft IMEs since Chrome won't do anything about those.
    if (IsMicrosoftIme(guid))
      continue;

    base::FilePath dll_path = GetInprocServerDllPath(guid);
    if (dll_path.empty())
      continue;

    uint32_t size_of_image = 0;
    uint32_t time_date_stamp = 0;
    if (!GetModuleImageSizeAndTimeDateStamp(dll_path, &size_of_image,
                                            &time_date_stamp)) {
      continue;
    }

    nb_imes++;
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(on_ime_enumerated, dll_path, size_of_image,
                                  time_date_stamp));
  }

  task_runner->PostTask(FROM_HERE, std::move(on_enumeration_finished));

  base::UmaHistogramCounts100("ThirdPartyModules.InputMethodEditorsCount",
                              nb_imes);
}

}  // namespace

const wchar_t kImeRegistryKey[] = L"SOFTWARE\\Microsoft\\CTF\\TIP";

void EnumerateInputMethodEditors(OnImeEnumeratedCallback on_ime_enumerated,
                                 base::OnceClosure on_enumeration_finished) {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&EnumerateImesOnBlockingSequence,
                     base::SequencedTaskRunner::GetCurrentDefault(),
                     std::move(on_ime_enumerated),
                     std::move(on_enumeration_finished)));
}
