// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_TRANSLATE_KIT_CLIENT_H_
#define COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_TRANSLATE_KIT_CLIENT_H_

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_native_library.h"
#include "base/types/pass_key.h"
#include "components/services/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/services/on_device_translation/translate_kit_structs.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace on_device_translation {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(LoadTranslateKitResult)
enum class LoadTranslateKitResult {
  kUnknown = 0,
  // Success to load TranslateKit library.
  kSuccess = 1,
  // Fails due to invalid TranslateKit binary.
  kInvalidBinary = 2,
  // Success to load TranslateKit binary but fails due to invalid function
  // pointers.
  kInvalidFunctionPointer = 3,
  kMaxValue = kInvalidFunctionPointer,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ai/enums.xml:LoadTranslateKitResult)

// The client that wraps the plain C-style interfaces between Chrome and the
// TranslateKit. Changes to the interfaces must be backwards compatible and
// reflected in the Google3-side definition.
class TranslateKitClient {
 public:
  // Translator provides access to translation functionality.
  class Translator {
   public:
    virtual ~Translator() = default;
    virtual std::optional<std::string> Translate(const std::string& text) = 0;
  };

  static TranslateKitClient* Get();

  TranslateKitClient(const base::FilePath& library_path,
                     base::PassKey<TranslateKitClient>);
  ~TranslateKitClient();

  // Not copyable.
  TranslateKitClient(const TranslateKitClient&) = delete;
  TranslateKitClient& operator=(const TranslateKitClient&) = delete;

  void SetConfig(mojom::OnDeviceTranslationServiceConfigPtr config);

  // Returns if the translation from `source_lang` to `target_lang` is
  // supported.
  bool CanTranslate(const std::string& source_lang,
                    const std::string& target_lang);

  // Returns a Translator instance for the given pair of languages.
  // Returns null if either the language pair is not supported or fails to
  // to create such Translator.
  Translator* GetTranslator(const std::string& source_lang,
                            const std::string& target_lang);

 private:
  // TranslatorImpl manages an instance of translator created by TranslateKit
  // library and provides access to its translation functionality.
  class TranslatorImpl : public Translator {
   public:
    static std::unique_ptr<TranslatorImpl> MaybeCreate(
        TranslateKitClient* client,
        const std::string& source_lang,
        const std::string& target_lang);

    TranslatorImpl(base::PassKey<TranslatorImpl>,
                   TranslateKitClient* client,
                   std::uintptr_t translator_ptr);
    ~TranslatorImpl() override;
    // Not copyable.
    TranslatorImpl(const TranslatorImpl&) = delete;
    TranslatorImpl& operator=(const TranslatorImpl&) = delete;

    std::optional<std::string> Translate(const std::string& text) override;

   private:
    // Guaranteed to exist, as `client_` owns `this`.
    raw_ptr<TranslateKitClient> client_;
    // A pointer to a Translator instance created by the TranslateKit.
    // It should only be instantiated and deleted by the TranslateKit library.
    std::uintptr_t translator_ptr_;
  };

  static bool FileExists(const char* file_name,
                         size_t file_name_size,
                         bool* is_directory,
                         std::uintptr_t user_data);
  static std::uintptr_t OpenForReadOnlyMemoryMap(const char* file_name,
                                                 size_t file_name_size,
                                                 std::uintptr_t user_data);

  bool FileExistsImpl(const char* file_name,
                      size_t file_name_size,
                      bool* is_directory);
  std::uintptr_t OpenForReadOnlyMemoryMapImpl(const char* file_name,
                                              size_t file_name_size);

  // Initializes the TranslateKit instance only when first needed, provided the
  // underlying library has loaded successfully. Returns true if initialization
  // was successful, false otherwise.
  bool MaybeInitialize();

  // The TranslateKit binary.
  base::ScopedNativeLibrary lib_;

  std::uintptr_t kit_ptr_ = 0;
  bool failed_to_initialize_ = false;

  // The results after attempting to load `lib_`.
  LoadTranslateKitResult load_lib_result_ = LoadTranslateKitResult::kUnknown;

  using TranslatorKey = std::pair<std::string, std::string>;
  // Manages all instances of `Translator` created by this client.
  std::map<TranslatorKey, std::unique_ptr<Translator>> translators_;

  // WARNING:
  // Changes to the below interfaces must be backwards compatible and
  // reflected in the Google3-side definition.

  typedef bool (*FileExistsFn)(const char* file_name,
                               size_t file_name_size,
                               bool* is_directory,
                               std::uintptr_t user_data);
  typedef std::uintptr_t (*OpenForReadOnlyMemoryMapFn)(
      const char* file_name,
      size_t file_name_size,
      std::uintptr_t user_data);
  typedef void (*DeleteReadOnlyMemoryRegionFn)(std::uintptr_t memory_map_ptr,
                                               std::uintptr_t user_data);
  typedef const void* (*ReadOnlyMemoryRegionDataFn)(
      std::uintptr_t memory_map_ptr,
      std::uintptr_t user_data);
  typedef uint64_t (*ReadOnlyMemoryRegionLengthFn)(
      std::uintptr_t memory_map_ptr,
      std::uintptr_t user_data);

  typedef void (*InitializeStorageBackendFn)(
      FileExistsFn file_exists,
      OpenForReadOnlyMemoryMapFn open_for_read_only_memory_map,
      DeleteReadOnlyMemoryRegionFn delete_read_only_memory_region,
      ReadOnlyMemoryRegionDataFn read_only_memory_region_data,
      ReadOnlyMemoryRegionLengthFn read_only_memory_region_length,
      std::uintptr_t user_data);
  InitializeStorageBackendFn initialize_storage_backend_fnc_;

  typedef std::uintptr_t (*CreateTranslateKitFn)();
  CreateTranslateKitFn create_translate_kit_fnc_;

  typedef void (*DeleteTranslateKitFn)(std::uintptr_t);
  DeleteTranslateKitFn delete_tanslate_kit_fnc_;

  typedef bool (*TranslateKitSetLanguagePackagesFn)(
      uintptr_t kit_ptr,
      TranslateKitSetLanguagePackagesArgs args);
  TranslateKitSetLanguagePackagesFn set_language_packages_func_;

  typedef uintptr_t (*TranslateKitCreateTranslatorFn)(std::uintptr_t,
                                                      TranslateKitLanguage,
                                                      TranslateKitLanguage);
  TranslateKitCreateTranslatorFn translate_kit_create_translator_func_;

  typedef void (*DeleteTranslatorFn)(std::uintptr_t);
  DeleteTranslatorFn delete_translator_fnc_;

  typedef void (*TranslateCallbackFn)(TranslateKitOutputText, std::uintptr_t);
  typedef bool (*TranslatorTranslateFn)(std::uintptr_t translator_ptr,
                                        TranslateKitInputText,
                                        TranslateCallbackFn,
                                        std::uintptr_t user_data);
  TranslatorTranslateFn translator_translate_func_;

  mojo::Remote<mojom::FileOperationProxy> file_operation_proxy_;
};

}  // namespace on_device_translation

#endif  // COMPONENTS_SERVICES_ON_DEVICE_TRANSLATION_TRANSLATE_KIT_CLIENT_H_
