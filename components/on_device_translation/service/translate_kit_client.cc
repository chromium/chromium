// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/on_device_translation/service/translate_kit_client.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/memory_coordinator/traits.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/timer/elapsed_timer.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "components/on_device_translation/features.h"
#include "components/on_device_translation/service/proto/translate_kit_api.pb.h"
#include "components/on_device_translation/service/translate_kit_structs.h"

#if BUILDFLAG(IS_ANDROID)
#include <malloc.h>
#endif  // BUILDFLAG(IS_ANDROID)

namespace on_device_translation {
namespace {

using mojom::CreateTranslatorResult;

constexpr std::string_view kModelFileOpenErrorMetricName =
    "Translate.OnDeviceTranslation.ModelFileOpenError";
constexpr std::string_view kModelFileMmapSuccessMetricName =
    "Translate.OnDeviceTranslation.ModelFileMmapSuccess";

constexpr std::string_view kModelUnloadReasonMetricName =
    "AI.Translation.ModelUnloadReason";
constexpr std::string_view kModelReloadSuccessMetricName =
    "AI.Translation.ModelReloadSuccess";
constexpr std::string_view kModelReloadTimeMetricName =
    "AI.Translation.ModelReloadTime";

constexpr std::string_view kMemoryConsumerName = "TranslateKitClient";

constexpr base::MemoryConsumerTraits kMemoryConsumerTraits(
    // Each loaded language pair holds tens of MB, and a page can load several
    // pairs, so the total can reach hundreds of MB.
    base::MemoryConsumerTraits::EstimatedMemoryUsage::kLarge,
    // Deleting a TranslateKit translator frees its model.
    base::MemoryConsumerTraits::ReleaseMemoryDataAccess::kNone,
    // The model loads again from the same files.
    base::MemoryConsumerTraits::InformationRetention::kLossless,
    base::MemoryConsumerTraits::ExecutionType::kSynchronous,
    // `OnReleaseMemory()` reads the memory limit, and unloads all models only
    // under moderate or critical pressure.
    base::MemoryConsumerTraits::SupportsMemoryLimit::kYes,
    // The next translation loads the model from disk.
    base::MemoryConsumerTraits::RecreateMemoryCost::kExpensive,
    // Each notification unloads the models that were loaded since the last
    // one.
    base::MemoryConsumerTraits::IsStateful::kNo);

// Returns the free pages of the system allocator to the OS. TranslateKit
// allocates its model buffers with the system allocator, which on Android
// keeps freed pages for a while. Called after models unload, when the service
// is idle.
void ReleaseFreeSystemHeapMemory() {
#if BUILDFLAG(IS_ANDROID)
  mallopt(M_PURGE, 0);
#endif  // BUILDFLAG(IS_ANDROID)
}

// Logs UMA after an attempt to load the TranslateKit binary.
void LogLoadTranslateKitResult(LoadTranslateKitResult result,
                               const base::NativeLibraryLoadError* error) {
  base::UmaHistogramEnumeration("AI.Translation.LoadTranslateKitResult",
                                result);
#if BUILDFLAG(IS_WIN)
  if (result == LoadTranslateKitResult::kInvalidBinary) {
    base::UmaHistogramSparse("AI.Translation.LoadTranslateKitErrorCode",
                             error->code);
  }
#endif  // BUILDFLAG(IS_WIN)
}

// This method is used to receive the result from TranslatorTranslate() method.
void TranslateCallback(TranslateKitOutputText result,
                       std::uintptr_t user_data) {
  std::string* output = reinterpret_cast<std::string*>(user_data);
  CHECK(output);
  CHECK(result.buffer);
  *output = std::string(result.buffer, result.buffer_size);
}

void DeleteReadOnlyMemoryRegion(std::uintptr_t memory_map_ptr,
                                std::uintptr_t user_data) {
  CHECK(memory_map_ptr);
  delete reinterpret_cast<base::MemoryMappedFile*>(memory_map_ptr);
}

const void* ReadOnlyMemoryRegionData(std::uintptr_t memory_map_ptr,
                                     std::uintptr_t user_data) {
  CHECK(memory_map_ptr);
  return reinterpret_cast<base::MemoryMappedFile*>(memory_map_ptr)
      ->bytes()
      .data();
}

uint64_t ReadOnlyMemoryRegionLength(std::uintptr_t memory_map_ptr,
                                    std::uintptr_t user_data) {
  CHECK(memory_map_ptr);
  return reinterpret_cast<base::MemoryMappedFile*>(memory_map_ptr)
      ->bytes()
      .size();
}

void ParseFilePath(const char* file_name,
                   size_t file_name_size,
                   uint32_t& package_index,
                   base::FilePath& relative_path) {
  std::string path(file_name, file_name_size);
  // The TranslateKit only use ASCII paths.
  CHECK(base::IsStringASCII(path));
#if BUILDFLAG(IS_WIN)
  base::ReplaceChars(path, "/", "\\", &path);
#endif  // BUILDFLAG(IS_WIN)
  base::FilePath virtual_path = base::FilePath::FromASCII(path);
  // The TranslateKit doesn't use '..'.
  CHECK(!virtual_path.ReferencesParent());
  // The TranslateKit must use an absolute path.
  CHECK(virtual_path.IsAbsolute());
  const std::vector<base::FilePath::StringType> components =
      virtual_path.GetComponents();
#if BUILDFLAG(IS_WIN)
  // Windows:  "X:\0\bar"  ->  [ "X:", "\\", "0", "bar" ]
  //                                         ^^^ : component_idx = 2
  size_t component_idx = 2;
#else
  // Posix:  "/0/bar"  ->  [ "/", "0", "bar" ]
  //                              ^^^ : component_idx = 1
  size_t component_idx = 1;
#endif  // BUILDFLAG(IS_WIN)
  CHECK_GT(components.size(), component_idx + 1);
  CHECK(base::StringToUint(components[component_idx], &package_index));
  ++component_idx;
  base::FilePath result;
  for (; component_idx < components.size(); ++component_idx) {
    result = result.Append(components[component_idx]);
  }
  relative_path = result;
}

}  // namespace

// static
TranslateKitClient* TranslateKitClient::Get() {
  static base::NoDestructor<std::unique_ptr<TranslateKitClient>> client(
      std::make_unique<TranslateKitClient>(
          GetTranslateKitBinaryPathFromCommandLine(),
          base::PassKey<TranslateKitClient>()));
  return client->get();
}

// static
std::unique_ptr<TranslateKitClient> TranslateKitClient::CreateForTest(
    const base::FilePath& library_path) {
  return std::make_unique<TranslateKitClient>(
      library_path, base::PassKey<TranslateKitClient>());
}

TranslateKitClient::TranslateKitClient(const base::FilePath& library_path,
                                       base::PassKey<TranslateKitClient>)
    : lib_(library_path),
      get_translate_kit_version_func_(
          reinterpret_cast<GetTranslateKitVersionFn>(
              lib_.GetFunctionPointer("GetTranslateKitVersion"))),
      initialize_storage_backend_fnc_(
          reinterpret_cast<InitializeStorageBackendFn>(
              lib_.GetFunctionPointer("InitializeStorageBackend"))),
      create_translate_kit_fnc_(reinterpret_cast<CreateTranslateKitFn>(
          lib_.GetFunctionPointer("CreateTranslateKit"))),
      delete_tanslate_kit_fnc_(reinterpret_cast<DeleteTranslateKitFn>(
          lib_.GetFunctionPointer("DeleteTranslateKit"))),
      set_language_packages_func_(
          reinterpret_cast<TranslateKitSetLanguagePackagesFn>(
              lib_.GetFunctionPointer("TranslateKitSetLanguagePackages"))),
      translate_kit_create_translator_func_(
          reinterpret_cast<TranslateKitCreateTranslatorFn>(
              lib_.GetFunctionPointer("TranslateKitCreateTranslator"))),
      delete_translator_fnc_(reinterpret_cast<DeleteTranslatorFn>(
          lib_.GetFunctionPointer("DeleteTranslator"))),
      translator_translate_func_(reinterpret_cast<TranslatorTranslateFn>(
          lib_.GetFunctionPointer("TranslatorTranslate"))),
      translate_kit_sentence_split_func_(
          reinterpret_cast<TranslateKitSplitSentencesFn>(
              lib_.GetFunctionPointer("TranslateKitSplitSentences"))),
      model_idle_unload_timeout_(kTranslationAPIModelIdleUnloadTimeout.Get()) {
  LogLoadTranslateKitResult(CheckLoadTranslateKitResult(), lib_.GetError());
  if (model_idle_unload_timeout_.is_positive()) {
    memory_consumer_registration_.emplace(
        kMemoryConsumerName, kMemoryConsumerTraits, this,
        // `Get()` leaks the client, so it may outlive the registry.
        base::MemoryConsumerRegistration::CheckUnregister::kDisabled);
  }
}

LoadTranslateKitResult TranslateKitClient::CheckLoadTranslateKitResult() {
  if (!lib_.is_valid()) {
    maybe_kit_ptr_ =
        base::unexpected(CreateTranslatorResult::kErrorInvalidBinary);
    return LoadTranslateKitResult::kInvalidBinary;
  }

  if (!get_translate_kit_version_func_ || !initialize_storage_backend_fnc_ ||
      !create_translate_kit_fnc_ || !delete_tanslate_kit_fnc_ ||
      !set_language_packages_func_ || !translate_kit_create_translator_func_ ||
      !delete_translator_fnc_ || !translator_translate_func_) {
    maybe_kit_ptr_ =
        base::unexpected(CreateTranslatorResult::kErrorInvalidFunctionPointer);
    return LoadTranslateKitResult::kInvalidFunctionPointer;
  }

  if (!IsTranslateKitVersionValid()) {
    maybe_kit_ptr_ =
        base::unexpected(CreateTranslatorResult::kErrorInvalidVersion);
    return LoadTranslateKitResult::kInvalidVersion;
  }

  return LoadTranslateKitResult::kSuccess;
}

DISABLE_CFI_DLSYM
bool TranslateKitClient::IsTranslateKitVersionValid() {
  std::string version_buffer(kTranslationAPILibraryVersionStringSize, '\0');
  TranslateKitVersion version{version_buffer.data(), version_buffer.size()};
  if (!get_translate_kit_version_func_(&version)) {
    return false;
  }

  std::string_view version_string(version.buffer, version.buffer_size);
  return IsValidTranslateKitVersion(version_string);
}

DISABLE_CFI_DLSYM
bool TranslateKitClient::MaybeInitialize() {
  if (maybe_kit_ptr_.has_value() && *maybe_kit_ptr_) {
    // Already successfully initialized.
    return true;
  }
  if (!maybe_kit_ptr_.has_value()) {
    // An error occurred while loading the TranslateKit binary or the previous
    // initialization failed.
    return false;
  }
  initialize_storage_backend_fnc_(
      &TranslateKitClient::FileExists,
      &TranslateKitClient::OpenForReadOnlyMemoryMap,
      &DeleteReadOnlyMemoryRegion, &ReadOnlyMemoryRegionData,
      &ReadOnlyMemoryRegionLength, reinterpret_cast<std::uintptr_t>(this));

  maybe_kit_ptr_ = create_translate_kit_fnc_();
  if (!*maybe_kit_ptr_) {
    maybe_kit_ptr_ =
        base::unexpected(CreateTranslatorResult::kErrorFailedToInitialize);
    return false;
  }
  return true;
}

DISABLE_CFI_DLSYM
void TranslateKitClient::SetConfig(
    mojom::OnDeviceTranslationServiceConfigPtr config) {
  if (!MaybeInitialize()) {
    return;
  }

  // When `file_operation_proxy_` is set, need to reset `file_operation_proxy_`
  // before binding the new one. This happens when SetConfig() is called again
  // for the new config.
  file_operation_proxy_.reset();
  file_operation_proxy_.Bind(std::move(config->file_operation_proxy));
  chrome::on_device_translation::TranslateKitLanguagePackageConfig config_proto;
  size_t index = 0;
  for (const auto& package : config->packages) {
    // Generate a virtual absolute file path for the package.
    // On Windows, set the package path to a fake drive letter 'X:' to avoid
    // the file path validation in the TranslateKit.
    const std::string package_path =
#if BUILDFLAG(IS_WIN)
        base::StrCat({"X:\\", base::NumberToString(index++)});
#else
        base::StrCat({"/", base::NumberToString(index++)});
#endif  // BUILDFLAG(IS_WIN)
    auto* new_package = config_proto.add_packages();
    new_package->set_language1(package->language1);
    new_package->set_language2(package->language2);
    new_package->set_package_path(package_path);
  }

  const std::string packages_str = config_proto.SerializeAsString();
  CHECK(set_language_packages_func_(
      *maybe_kit_ptr_,
      TranslateKitSetLanguagePackagesArgs{packages_str.c_str(),
                                          packages_str.size()}))
      << "Failed to set config";
}

DISABLE_CFI_DLSYM
TranslateKitClient::~TranslateKitClient() {
  memory_consumer_registration_.reset();

  // The translators delete their TranslateKit translators, which needs the
  // kit, so delete them first.
  translators_.clear();
  if (!maybe_kit_ptr_.has_value() || !*maybe_kit_ptr_) {
    return;
  }
  delete_tanslate_kit_fnc_(*maybe_kit_ptr_);
  maybe_kit_ptr_ = 0;
}

bool TranslateKitClient::CanTranslate(const std::string& source_lang,
                                      const std::string& target_lang) {
  if (!maybe_kit_ptr_.has_value()) {
    return false;
  }
  CHECK(*maybe_kit_ptr_) << "SetConfig must have been called";
  CHECK(file_operation_proxy_);

  return TranslateKitClient::GetTranslator(source_lang, target_lang)
      .has_value();
}

base::expected<TranslateKitClient::Translator*, CreateTranslatorResult>
TranslateKitClient::GetTranslator(const std::string& source_lang,
                                  const std::string& target_lang) {
  if (!maybe_kit_ptr_.has_value()) {
    CHECK_NE(maybe_kit_ptr_.error(), CreateTranslatorResult::kSuccess);
    return base::unexpected(maybe_kit_ptr_.error());
  }
  CHECK(*maybe_kit_ptr_) << "SetConfig must have been called";
  CHECK(file_operation_proxy_);

  TranslatorKey key(source_lang, target_lang);
  if (auto it = translators_.find(key); it != translators_.end()) {
    return it->second.get();
  }
  auto translator = TranslatorImpl::MaybeCreate(this, source_lang, target_lang);
  if (!translator) {
    return base::unexpected(
        CreateTranslatorResult::kErrorFailedToCreateTranslator);
  }
  auto raw_translator_ptr = translator.get();
  translators_.emplace(std::move(key), std::move(translator));
  return raw_translator_ptr;
}

void TranslateKitClient::UnloadAllTranslatorModels(
    TranslateKitModelUnloadReason reason) {
  bool unloaded = false;
  for (auto& [key, translator] : translators_) {
    auto* translator_impl = static_cast<TranslatorImpl*>(translator.get());
    if (translator_impl->IsModelLoaded()) {
      unloaded = true;
      translator_impl->UnloadModel(reason);
    }
  }

  // Purging walks the allocator, so skip it when nothing was freed.
  if (unloaded) {
    ReleaseFreeSystemHeapMemory();
  }
}

size_t TranslateKitClient::GetLoadedTranslatorCountForTesting() const {
  return std::ranges::count_if(translators_, [](const auto& entry) {
    return static_cast<const TranslatorImpl*>(entry.second.get())
        ->IsModelLoaded();
  });
}

void TranslateKitClient::OnReleaseMemory() {
  if (memory_limit() <= base::MemoryLimit::ModeratePressureThreshold()) {
    UnloadAllTranslatorModels(TranslateKitModelUnloadReason::kMemoryPressure);
  }
}

void TranslateKitClient::OnUpdateMemoryLimit() {}

// static
DISABLE_CFI_DLSYM
std::uintptr_t TranslateKitClient::TranslatorImpl::CreateTranslateKitTranslator(
    TranslateKitClient* client,
    const std::string& source_lang,
    const std::string& target_lang) {
  CHECK(client->translate_kit_create_translator_func_);
  return client->translate_kit_create_translator_func_(
      *client->maybe_kit_ptr_,
      TranslateKitLanguage(source_lang.c_str(), source_lang.length()),
      TranslateKitLanguage(target_lang.c_str(), target_lang.length()));
}

// static
std::unique_ptr<TranslateKitClient::TranslatorImpl>
TranslateKitClient::TranslatorImpl::MaybeCreate(
    TranslateKitClient* client,
    const std::string& source_lang,
    const std::string& target_lang) {
  std::uintptr_t translator_ptr =
      CreateTranslateKitTranslator(client, source_lang, target_lang);
  if (!translator_ptr) {
    return nullptr;
  }
  return std::make_unique<TranslatorImpl>(base::PassKey<TranslatorImpl>(),
                                          client, source_lang, target_lang,
                                          translator_ptr);
}

TranslateKitClient::TranslatorImpl::TranslatorImpl(
    base::PassKey<TranslatorImpl>,
    TranslateKitClient* client,
    const std::string& source_lang,
    const std::string& target_lang,
    std::uintptr_t translator_ptr)
    : client_(client),
      source_lang_(source_lang),
      target_lang_(target_lang),
      translator_ptr_(translator_ptr) {
  RestartIdleUnloadTimer();
}

TranslateKitClient::TranslatorImpl::~TranslatorImpl() {
  DeleteTranslateKitTranslator();
}

void TranslateKitClient::TranslatorImpl::UnloadModel(
    TranslateKitModelUnloadReason reason) {
  if (translator_ptr_) {
    base::UmaHistogramEnumeration(kModelUnloadReasonMetricName, reason);
  }
  DeleteTranslateKitTranslator();
}

DISABLE_CFI_DLSYM
void TranslateKitClient::TranslatorImpl::DeleteTranslateKitTranslator() {
  idle_unload_timer_.Stop();
  if (!translator_ptr_) {
    return;
  }
  CHECK(client_->delete_translator_fnc_);
  client_->delete_translator_fnc_(std::exchange(translator_ptr_, 0));
}

bool TranslateKitClient::TranslatorImpl::MaybeLoadModel() {
  if (translator_ptr_) {
    return true;
  }
  base::ElapsedTimer timer;
  translator_ptr_ =
      CreateTranslateKitTranslator(client_, source_lang_, target_lang_);
  const bool success = translator_ptr_ != 0;
  base::UmaHistogramBoolean(kModelReloadSuccessMetricName, success);
  if (success) {
    base::UmaHistogramMediumTimes(kModelReloadTimeMetricName, timer.Elapsed());
  }
  return success;
}

void TranslateKitClient::TranslatorImpl::RestartIdleUnloadTimer() {
  if (!client_->model_idle_unload_timeout_.is_positive()) {
    return;
  }

  // Unretained is safe because `this` owns the timer.
  idle_unload_timer_.Start(
      FROM_HERE, client_->model_idle_unload_timeout_,
      base::BindOnce(&TranslatorImpl::OnIdleTimeout, base::Unretained(this)));
}

void TranslateKitClient::TranslatorImpl::OnIdleTimeout() {
  UnloadModel(TranslateKitModelUnloadReason::kIdle);
  ReleaseFreeSystemHeapMemory();
}

DISABLE_CFI_DLSYM
std::optional<std::string> TranslateKitClient::TranslatorImpl::Translate(
    const std::string& text) {
  CHECK(client_->translator_translate_func_);
  if (!MaybeLoadModel()) {
    return std::nullopt;
  }
  const bool translated = client_->translator_translate_func_(
      translator_ptr_, TranslateKitInputText(text.c_str(), text.length()),
      TranslateCallback, reinterpret_cast<uintptr_t>(&text));

  // Count the idle time from the end of the call. One call can run longer
  // than the timeout, and a timer that started before it would unload the
  // model as soon as the call returns.
  RestartIdleUnloadTimer();
  if (!translated) {
    return std::nullopt;
  }
  return text;
}

DISABLE_CFI_DLSYM
std::vector<std::string> TranslateKitClient::TranslatorImpl::SplitSentences(
    const std::string& text) {
  std::vector<std::string> sentences;
  if (!client_->translate_kit_sentence_split_func_) {
    sentences.push_back(text);
    return sentences;
  }

  auto sentence_callback = [](TranslateKitOutputText result,
                              std::uintptr_t user_data) {
    std::vector<std::string>* sentences =
        reinterpret_cast<std::vector<std::string>*>(user_data);
    CHECK(sentences);
    CHECK(result.buffer);
    std::string sentence(result.buffer, result.buffer_size);
    sentences->push_back(std::move(sentence));
  };

  // The library sentence splitter expects a primary language subtag and
  // rejects tags with subtags such as zh-Hant. Sentence boundaries are the
  // same across scripts and regions of a language, so pass the subtag before
  // any hyphen.
  const std::string_view split_lang =
      std::string_view(source_lang_).substr(0, source_lang_.find('-'));
  if (!client_->translate_kit_sentence_split_func_(
          TranslateKitInputText(text.c_str(), text.length()),
          TranslateKitLanguage(split_lang.data(), split_lang.length()),
          sentence_callback, reinterpret_cast<std::uintptr_t>(&sentences))) {
    return std::vector<std::string>{text};
  }

  if (sentences.empty() && !text.empty()) {
    // If the splitter emits zero sentences for non-empty text, fall back to
    // translating the whole text as one chunk because an empty list signals a
    // dropped service call to the browser.
    return std::vector<std::string>{text};
  }
  return sentences;
}

// static
bool TranslateKitClient::FileExists(const char* file_name,
                                    size_t file_name_size,
                                    bool* is_directory,
                                    std::uintptr_t user_data) {
  CHECK(file_name);
  CHECK(is_directory);
  CHECK(user_data);
  return reinterpret_cast<TranslateKitClient*>(user_data)->FileExistsImpl(
      file_name, file_name_size, is_directory);
}
bool TranslateKitClient::FileExistsImpl(const char* file_name,
                                        size_t file_name_size,
                                        bool* is_directory) {
  uint32_t package_index = 0;
  base::FilePath relative_path;
  ParseFilePath(file_name, file_name_size, package_index, relative_path);
  bool exists = false;
  CHECK(file_operation_proxy_);
  file_operation_proxy_->FileExists(package_index, relative_path, &exists,
                                    is_directory);
  return exists;
}

// static
std::uintptr_t TranslateKitClient::OpenForReadOnlyMemoryMap(
    const char* file_name,
    size_t file_name_size,
    std::uintptr_t user_data) {
  CHECK(file_name);
  CHECK(user_data);
  return reinterpret_cast<TranslateKitClient*>(user_data)
      ->OpenForReadOnlyMemoryMapImpl(file_name, file_name_size);
}

std::uintptr_t TranslateKitClient::OpenForReadOnlyMemoryMapImpl(
    const char* file_name,
    size_t file_name_size) {
  uint32_t package_index = 0;
  base::FilePath relative_path;
  ParseFilePath(file_name, file_name_size, package_index, relative_path);
  base::File file;
  CHECK(file_operation_proxy_);
  file_operation_proxy_->Open(package_index, relative_path, &file);
  if (!file.IsValid()) {
    base::UmaHistogramExactLinear(kModelFileOpenErrorMetricName,
                                  -file.error_details(),
                                  -base::File::FILE_ERROR_MAX);
    return 0;
  }
  auto mapped_file = std::make_unique<base::MemoryMappedFile>();
  const bool mmap_success = mapped_file->Initialize(std::move(file));
  base::UmaHistogramBoolean(kModelFileMmapSuccessMetricName, mmap_success);
  if (!mmap_success) {
    return 0;
  }
  return reinterpret_cast<std::uintptr_t>(mapped_file.release());
}

}  // namespace on_device_translation
