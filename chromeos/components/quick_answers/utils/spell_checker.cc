// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/utils/spell_checker.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/common/chrome_paths.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_process_host.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace quick_answers {
namespace {

constexpr char kDownloadServerUrl[] =
    "https://redirector.gvt1.com/edgedl/chrome/dict/";

constexpr net::NetworkTrafficAnnotationTag kNetworkTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("quick_answers_spellchecker", R"(
          semantics {
            sender: "Quick answers Spellchecker"
            description:
              "Download dictionary for Quick answers feature if necessary."
            trigger: "Quick answers feature enabled."
            data:
              "The spell checking language identifier. No user identifier is "
              "sent."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: NO
            setting:
              "Quick Answers can be enabled/disabled in ChromeOS Settings and"
              "is subject to eligibility requirements."
            chrome_policy {
              QuickAnswersEnabled {
                QuickAnswersEnabled: false
              }
            }
          })");

constexpr int kMaxRetries = 3;

base::FilePath GetDictionaryFilePath(const std::string& language) {
  base::FilePath dict_dir;
  base::PathService::Get(chrome::DIR_APP_DICTIONARIES, &dict_dir);
  base::FilePath dict_path =
      spellcheck::GetVersionedFileName(language, dict_dir);
  return dict_path;
}

GURL GetDictionaryURL(const std::string& file_name) {
  return GURL(std::string(kDownloadServerUrl) + base::ToLowerASCII(file_name));
}

bool SaveDictionaryData(std::unique_ptr<std::string> data,
                        const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Create a temporary file.
  base::FilePath tmp_path;
  if (!base::CreateTemporaryFileInDir(file_path.DirName(), &tmp_path)) {
    LOG(ERROR) << "Failed to create a temporary file.";
    return false;
  }

  // Write to the temporary file.
  size_t bytes_written =
      base::WriteFile(tmp_path, data->data(), data->length());
  if (bytes_written != data->length()) {
    base::DeleteFile(tmp_path);
    LOG(ERROR) << "Failed to write dictionary data to the temporary file";
    return false;
  }

  // Atomically rename the temporary file to become the real one.
  return base::ReplaceFile(tmp_path, file_path, nullptr);
}

void RemoveDictionaryFle(const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::DeleteFile(file_path);
}

}  // namespace

SpellChecker::SpellChecker(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {
  quick_answers_state_observation_.Observe(QuickAnswersState::Get());
}

SpellChecker::~SpellChecker() = default;

void SpellChecker::CheckSpelling(const std::string& word,
                                 CheckSpellingCallback callback) {
  if (!dictionary_initialized_) {
    std::move(callback).Run(false);
    return;
  }

  dictionary_->CheckSpelling(word, std::move(callback));
}

void SpellChecker::OnSettingsEnabled(bool enabled) {
  feature_enabled_ = enabled;

  // Reset spell check service if the feature is disabled.
  if (!enabled) {
    dictionary_.reset();
    service_.reset();
    return;
  }

  if (!dictionary_file_path_.empty())
    InitializeDictionary();
}

void SpellChecker::OnApplicationLocaleReady(const std::string& locale) {
  dictionary_file_path_ = GetDictionaryFilePath(locale);

  if (feature_enabled_)
    InitializeDictionary();
}

void SpellChecker::InitializeDictionary() {
  DCHECK(!dictionary_file_path_.empty());

  // If the dictionary is not available, try to download it from the server.
  if (!base::PathExists(dictionary_file_path_)) {
    auto url =
        GetDictionaryURL(dictionary_file_path_.BaseName().MaybeAsASCII());

    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = url;
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                               kNetworkTrafficAnnotationTag);
    // TODO(b/226221138): Probably use |DownloadToTempFile| instead.
    loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory_.get(),
        base::BindOnce(&SpellChecker::OnSimpleURLLoaderComplete,
                       base::Unretained(this)));
    return;
  }

  InitializeSpellCheckService();
}

void SpellChecker::InitializeSpellCheckService() {
  DCHECK(base::PathExists(dictionary_file_path_));

  if (!service_) {
    service_ = content::ServiceProcessHost::Launch<mojom::SpellCheckService>(
        content::ServiceProcessHost::Options()
            .WithDisplayName("Quick answers spell check service")
            .Pass());
  }

  base::File file(dictionary_file_path_,
                  base::File::FLAG_READ | base::File::FLAG_OPEN);

  service_->CreateDictionary(file.Duplicate(),
                             base::BindOnce(&SpellChecker::OnDictionaryCreated,
                                            base::Unretained(this)));
}

void SpellChecker::OnSimpleURLLoaderComplete(
    std::unique_ptr<std::string> data) {
  int response_code = -1;
  if (loader_->ResponseInfo() && loader_->ResponseInfo()->headers)
    response_code = loader_->ResponseInfo()->headers->response_code();

  if (loader_->NetError() != net::OK || ((response_code / 100) != 2)) {
    LOG(ERROR) << "Failed to download the dictionary.";
    MaybeRetryInitialize();
    return;
  }

  // Basic sanity check on the dictionary data.
  if (!data || data->size() < 4 || data->compare(0, 4, "BDic") != 0) {
    LOG(ERROR) << "Downloaded dictionary data is empty or broken.";
    MaybeRetryInitialize();
    return;
  }

  if (!SaveDictionaryData(std::move(data), dictionary_file_path_)) {
    MaybeRetryInitialize();
    return;
  }

  InitializeSpellCheckService();
}

void SpellChecker::OnDictionaryCreated(
    mojo::PendingRemote<mojom::SpellCheckDictionary> dictionary) {
  dictionary_.reset();

  if (dictionary.is_valid()) {
    dictionary_.Bind(std::move(dictionary));
    dictionary_initialized_ = true;
    return;
  }

  MaybeRetryInitialize();
}

void SpellChecker::MaybeRetryInitialize() {
  RemoveDictionaryFle(dictionary_file_path_);

  if (num_retries_ >= kMaxRetries) {
    LOG(ERROR) << "Service initialize failed after max retries";
    service_.reset();
    return;
  }

  ++num_retries_;
  InitializeDictionary();
}

}  // namespace quick_answers
