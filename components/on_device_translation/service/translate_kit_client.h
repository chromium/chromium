// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ON_DEVICE_TRANSLATION_SERVICE_TRANSLATE_KIT_CLIENT_H_
#define COMPONENTS_ON_DEVICE_TRANSLATION_SERVICE_TRANSLATE_KIT_CLIENT_H_

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/scoped_native_library.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "components/on_device_translation/public/mojom/on_device_translation_service.mojom.h"
#include "components/on_device_translation/service/translate_kit_structs.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace on_device_translation {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(LoadTranslateKitResult)
enum class LoadTranslateKitResult {
  // kUnknown = 0,  Deprecated
  // Success to load TranslateKit library.
  kSuccess = 1,
  // Fails due to invalid TranslateKit binary.
  kInvalidBinary = 2,
  // Success to load TranslateKit binary but fails due to invalid function
  // pointers.
  kInvalidFunctionPointer = 3,
  // Success to load TranslateKit binary but the binary version is invalid.
  kInvalidVersion = 4,

  kMaxValue = kInvalidVersion,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ai/enums.xml:LoadTranslateKitResult)

// Why the translation service unloaded the TranslateKit model of a translator.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(TranslateKitModelUnloadReason)
enum class TranslateKitModelUnloadReason {
  // The translator was not used for `kTranslationAPIModelIdleUnloadTimeout`.
  kIdle = 0,
  // The process was under moderate or critical memory pressure.
  kMemoryPressure = 1,

  kMaxValue = kMemoryPressure,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/ai/enums.xml:TranslateKitModelUnloadReason)

// The client that wraps the plain C-style interfaces between Chrome and the
// TranslateKit. Changes to the interfaces must be backwards compatible and
// reflected in the Google3-side definition.
//
// When `kTranslationAPIModelIdleUnloadTimeout` is positive, a `Translator`
// unloads its TranslateKit model after being idle for that duration, and all
// models unload under memory pressure. The model
// loads again on the next Translate() call. All of this happens on the
// sequence that uses the client, so an unload never runs during a translation.
class TranslateKitClient : public base::MemoryConsumer {
 public:
  // Translator provides access to translation functionality.
  class Translator {
   public:
    virtual ~Translator() = default;
    virtual std::optional<std::string> Translate(const std::string& text) = 0;
    virtual std::vector<std::string> SplitSentences(
        const std::string& text) = 0;
  };

  static TranslateKitClient* Get();
  static std::unique_ptr<TranslateKitClient> CreateForTest(
      const base::FilePath& library_path);

  TranslateKitClient(const base::FilePath& library_path,
                     base::PassKey<TranslateKitClient>);
  ~TranslateKitClient() override;

  // Not copyable.
  TranslateKitClient(const TranslateKitClient&) = delete;
  TranslateKitClient& operator=(const TranslateKitClient&) = delete;

  void SetConfig(mojom::OnDeviceTranslationServiceConfigPtr config);

  // Returns if the translation from `source_lang` to `target_lang` is
  // supported.
  bool CanTranslate(const std::string& source_lang,
                    const std::string& target_lang);

  // Returns a Translator instance for the given pair of languages, or an error
  // of type `mojom::CreateTranslatorResult` if fails to create such Translator.
  base::expected<Translator*, mojom::CreateTranslatorResult> GetTranslator(
      const std::string& source_lang,
      const std::string& target_lang);

  // Returns the number of translators that have a loaded model.
  size_t GetLoadedTranslatorCountForTesting() const;

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
                   const std::string& source_lang,
                   const std::string& target_lang,
                   std::uintptr_t translator_ptr);
    ~TranslatorImpl() override;
    // Not copyable.
    TranslatorImpl(const TranslatorImpl&) = delete;
    TranslatorImpl& operator=(const TranslatorImpl&) = delete;

    std::optional<std::string> Translate(const std::string& text) override;
    std::vector<std::string> SplitSentences(const std::string& text) override;

    // Returns true if the TranslateKit translator, and so its model, is
    // loaded.
    bool IsModelLoaded() const { return translator_ptr_ != 0; }

    // Deletes the TranslateKit translator, which releases its model, and
    // records `reason` if the model was loaded. The next Translate() call
    // creates it again.
    void UnloadModel(TranslateKitModelUnloadReason reason);

   private:
    // Creates a TranslateKit translator. Returns 0 on failure.
    static std::uintptr_t CreateTranslateKitTranslator(
        TranslateKitClient* client,
        const std::string& source_lang,
        const std::string& target_lang);

    // Creates the TranslateKit translator if it was unloaded, and records
    // whether that worked and how long it took. Returns false if it fails.
    bool MaybeLoadModel();

    // Deletes the TranslateKit translator, if any.
    void DeleteTranslateKitTranslator();

    // Restarts the countdown to OnIdleTimeout(), if idle unload is enabled.
    void RestartIdleUnloadTimer();

    // Unloads the model and returns its freed memory to the OS.
    void OnIdleTimeout();

    // Guaranteed to exist, as `client_` owns `this`.
    raw_ptr<TranslateKitClient> client_;
    const std::string source_lang_;
    const std::string target_lang_;

    // A pointer to a Translator instance created by the TranslateKit, or 0
    // while the model is unloaded.
    // It should only be instantiated and deleted by the TranslateKit library.
    std::uintptr_t translator_ptr_;

    // Fires OnIdleTimeout() after the translator was not used for
    // `client_->model_idle_unload_timeout_`.
    base::OneShotTimer idle_unload_timer_;
  };

  // Unloads the TranslateKit models of all translators for `reason`. Each
  // translator loads its model again on its next Translate() call.
  void UnloadAllTranslatorModels(TranslateKitModelUnloadReason reason);

  // base::MemoryConsumer:
  void OnReleaseMemory() override;
  void OnUpdateMemoryLimit() override;

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

  // Checks if the TranslateKit binary is valid and all the function pointers
  // are valid. And sets the `maybe_kit_ptr_` to the error code if any.
  LoadTranslateKitResult CheckLoadTranslateKitResult();

  // Checks if the loaded TranslateKit version is valid.
  bool IsTranslateKitVersionValid();

  // Initializes the TranslateKit instance only when first needed, provided the
  // underlying library has loaded successfully. Returns true if initialization
  // was successful, false otherwise.
  bool MaybeInitialize();

  // The TranslateKit binary.
  base::ScopedNativeLibrary lib_;

  using TranslatorKey = std::pair<std::string, std::string>;
  // Manages all instances of `Translator` created by this client.
  std::map<TranslatorKey, std::unique_ptr<Translator>> translators_;

  // WARNING:
  // Changes to the below interfaces must be backwards compatible and
  // reflected in the Google3-side definition.
  typedef bool (*GetTranslateKitVersionFn)(TranslateKitVersion* version);
  GetTranslateKitVersionFn get_translate_kit_version_func_;

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

  typedef void (*SentenceSplitCallbackFn)(TranslateKitOutputText,
                                          std::uintptr_t);
  typedef bool (*TranslateKitSplitSentencesFn)(TranslateKitInputText,
                                               TranslateKitLanguage,
                                               SentenceSplitCallbackFn,
                                               std::uintptr_t user_data);
  TranslateKitSplitSentencesFn translate_kit_sentence_split_func_;

  // The pointer to the TranslateKit instance or 0 if not initialized or
  // error `mojom::CreateTranslatorResult` if failed to initialize.
  base::expected<std::uintptr_t, mojom::CreateTranslatorResult> maybe_kit_ptr_{
      0};

  mojo::Remote<mojom::FileOperationProxy> file_operation_proxy_;

  // How long a translator stays loaded without a translation. Zero turns idle
  // unload off.
  const base::TimeDelta model_idle_unload_timeout_;

  // Set when models unload under memory pressure. Declared last so that it
  // unregisters before the translators go away.
  std::optional<base::MemoryConsumerRegistration> memory_consumer_registration_;
};

}  // namespace on_device_translation

#endif  // COMPONENTS_ON_DEVICE_TRANSLATION_SERVICE_TRANSLATE_KIT_CLIENT_H_
