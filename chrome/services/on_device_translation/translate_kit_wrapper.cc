// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/on_device_translation/translate_kit_wrapper.h"

#include <optional>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/services/on_device_translation/public/cpp/features.h"

namespace on_device_translation {

TranslateKitWrapper::TranslateKitWrapper() = default;

TranslateKitWrapper::~TranslateKitWrapper() = default;

// static
void TranslateKitWrapper::GetInstance(
    base::OnceCallback<void(TranslateKitWrapper*)> callback) {
  static base::NoDestructor<TranslateKitWrapper> instance;
  TranslateKitWrapper* wrapper = instance.get();
  if (!wrapper->is_library_loaded_) {
    // Load the library if it was not loaded by the wrapper before. This will
    // only happen once.
    wrapper->Load(std::move(callback));
    return;
  }
  std::move(callback).Run(wrapper);
}

std::optional<TranslateFunc> TranslateKitWrapper::GetTranslateFunc(
    const std::string& source_lang,
    const std::string& target_lang) {
  std::optional<base::FilePath> run_files_path =
      GetTranslateLibraryRunFilesPath();
  if (!run_files_path) {
    return std::nullopt;
  }

  std::uintptr_t translator = create_translator_func_(run_files_path->value(),
                                                      source_lang, target_lang);
  return base::BindRepeating(
      [](std::string do_translation_func(std::uintptr_t, const std::string&),
         std::uintptr_t translator, const std::string& input) {
        return do_translation_func(translator, input);
      },
      do_translation_func_, translator);
}

bool TranslateKitWrapper::CanTranslate(const std::string& source_lang,
                                       const std::string& target_lang) {
  return is_language_supported_func_(source_lang) &&
         is_language_supported_func_(target_lang);
}

void TranslateKitWrapper::Load(
    base::OnceCallback<void(TranslateKitWrapper*)> callback) {
  // This needs to be done in a task runner with `MayBlock` trait.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&TranslateKitWrapper::LoadTranslateKit,
                     base::Unretained(this)),
      base::BindOnce(
          [](base::OnceCallback<void(TranslateKitWrapper*)> callback,
             TranslateKitWrapper* wrapper) {
            if (wrapper && wrapper->is_library_loaded_) {
              std::move(callback).Run(wrapper);
            } else {
              // Return nullptr if the wrapper fails to load the library.
              std::move(callback).Run(nullptr);
            }
          },
          std::move(callback), base::Unretained(this)));
}

std::optional<base::FilePath>
TranslateKitWrapper::GetTranslateKitLibraryBasePath() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(kTranslateKitDir)) {
    return std::nullopt;
  }
  return base::FilePath(command_line->GetSwitchValueASCII(kTranslateKitDir));
}

std::optional<base::FilePath>
TranslateKitWrapper::GetTranslateKitLibraryPath() {
  std::optional<base::FilePath> base_path = GetTranslateKitLibraryBasePath();
  if (base_path) {
    return base_path->AppendASCII(
        base::GetNativeLibraryName(kTranslateKitLibraryName));
  }
  return std::nullopt;
}

std::optional<base::FilePath>
TranslateKitWrapper::GetTranslateLibraryRunFilesPath() {
  std::optional<base::FilePath> base_path = GetTranslateKitLibraryBasePath();
  if (base_path) {
    return base_path->AppendASCII(base::StrCat(
        {base::GetNativeLibraryName(kTranslateKitLibraryName), ".runfiles"}));
  }
  return std::nullopt;
}

void TranslateKitWrapper::LoadTranslateKit() {
  base::NativeLibraryLoadError error;
  std::optional<base::FilePath> library_path = GetTranslateKitLibraryPath();
  if (!library_path) {
    DLOG(ERROR) << "Failed to load the library, path is not provided.";
    return;
  }

  base::NativeLibrary library =
      base::LoadNativeLibrary(library_path.value(), &error);
  if (!library) {
    DLOG(ERROR) << "Failed to load the library, path: " << library_path.value()
                << "; error: " << error.ToString();
    return;
  }
  translate_kit_library_ = base::ScopedNativeLibrary(library);
  // Call `InitGoogle()` before everything.
  reinterpret_cast<void (*)()>(
      translate_kit_library_.GetFunctionPointer("InitGoogle"))();

  is_language_supported_func_ = reinterpret_cast<bool (*)(const std::string&)>(
      translate_kit_library_.GetFunctionPointer("IsLanguageSupported"));
  create_translator_func_ = reinterpret_cast<std::uintptr_t (*)(
      const std::string&, const std::string&, const std::string&)>(
      translate_kit_library_.GetFunctionPointer("CreateTranslator"));
  do_translation_func_ =
      reinterpret_cast<std::string (*)(std::uintptr_t, const std::string&)>(
          translate_kit_library_.GetFunctionPointer("DoTranslation"));
  is_library_loaded_ = true;
}

}  // namespace on_device_translation
