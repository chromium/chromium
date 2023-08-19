// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/quick_answers/utils/spell_check_language.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
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

constexpr char kQuickAnswersDictionarySubDirName[] = "quick_answers";

constexpr net::NetworkTrafficAnnotationTag kNetworkTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("quick_answers_spellchecker", R"(
          semantics {
            sender: "Quick Answers Spellchecker"
            description:
              "Download spell checker dictionary for Quick Answers feature. "
              "The downloaded dictionaries are used to generate intents for "
              "selected text."
            trigger: "Eligible for Quick Answers feature"
            data:
              "The spell checking language identifier. No user identifier is "
              "sent other than user locales."
            destination: GOOGLE_OWNED_SERVICE
            internal {
              contacts {
                email: "assistive-eng@google.com"
              }
            }
            user_data {
              type: OTHER
            }
            last_reviewed: "2023-06-30"
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

constexpr int kMaxRetries = 1;

base::FilePath GetDictionaryDirectoryPath() {
  base::FilePath dict_dir;
  if (!base::PathService::Get(chrome::DIR_APP_DICTIONARIES, &dict_dir)) {
    return base::FilePath();
  }
  return dict_dir.AppendASCII(kQuickAnswersDictionarySubDirName);
}

bool EnsureDictionaryDirectoryExists() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::FilePath dict_dir = GetDictionaryDirectoryPath();

  if (dict_dir.empty()) {
    LOG(ERROR) << "Failed to resolve the dictionary directory path.";
    return false;
  }

  // `base::CreateDirectory` returns true if it successfully creates
  // the directory or if the directory already exists.
  return base::CreateDirectory(dict_dir);
}

GURL GetDictionaryURL(const std::string& file_name) {
  return GURL(std::string(kDownloadServerUrl) + base::ToLowerASCII(file_name));
}

base::File OpenDictionaryFile(const base::FilePath& file_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::File file(file_path, base::File::FLAG_READ | base::File::FLAG_OPEN);
  return file;
}

void CloseDictionaryFile(base::File file) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  file.Close();
}

bool RemoveDictionaryFile(const base::FilePath& file_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  return base::DeleteFile(file_path);
}

}  // namespace

SpellCheckLanguage::SpellCheckLanguage(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE})),
      url_loader_factory_(url_loader_factory) {}

SpellCheckLanguage::~SpellCheckLanguage() = default;

void SpellCheckLanguage::Initialize(const std::string& language) {
  language_ = language;

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&EnsureDictionaryDirectoryExists),
      base::BindOnce(&SpellCheckLanguage::OnDictionaryDirectoryExistsComplete,
                     weak_factory_.GetWeakPtr()));
}

void SpellCheckLanguage::CheckSpelling(const std::string& word,
                                       CheckSpellingCallback callback) {
  if (!dictionary_initialized_) {
    std::move(callback).Run(false);
    return;
  }

  dictionary_->CheckSpelling(word, std::move(callback));
}

void SpellCheckLanguage::InitializeSpellCheckService() {
  if (!service_) {
    service_ = content::ServiceProcessHost::Launch<mojom::SpellCheckService>(
        content::ServiceProcessHost::Options()
            .WithDisplayName("Quick answers spell check service")
            .Pass());
  }

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&OpenDictionaryFile, dictionary_file_path_),
      base::BindOnce(&SpellCheckLanguage::OnOpenDictionaryFileComplete,
                     weak_factory_.GetWeakPtr()));
}

void SpellCheckLanguage::OnSimpleURLLoaderComplete(base::FilePath tmp_path) {
  int response_code = -1;
  if (loader_->ResponseInfo() && loader_->ResponseInfo()->headers)
    response_code = loader_->ResponseInfo()->headers->response_code();

  if (loader_->NetError() != net::OK || ((response_code / 100) != 2)) {
    LOG(ERROR) << "Failed to download the dictionary.";
    MaybeRetryInitialize();
    return;
  }

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::PathExists, dictionary_file_path_),
      base::BindOnce(&SpellCheckLanguage::OnSaveDictionaryDataComplete,
                     weak_factory_.GetWeakPtr()));
}

void SpellCheckLanguage::OnDictionaryDirectoryExistsComplete(
    bool directory_exists) {
  if (!directory_exists) {
    LOG(ERROR) << "Failed to find or create the dictionary directory.";
    MaybeRetryInitialize();
    return;
  }

  base::FilePath dict_dir = GetDictionaryDirectoryPath();
  if (dict_dir.empty()) {
    LOG(ERROR) << "Failed to resolve the dictionary directory path.";
    MaybeRetryInitialize();
    return;
  }
  dictionary_file_path_ = spellcheck::GetVersionedFileName(language_, dict_dir);

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::PathExists, dictionary_file_path_),
      base::BindOnce(&SpellCheckLanguage::OnPathExistsComplete,
                     weak_factory_.GetWeakPtr()));
}

void SpellCheckLanguage::OnDictionaryCreated(
    mojo::PendingRemote<mojom::SpellCheckDictionary> dictionary) {
  dictionary_.reset();

  if (dictionary.is_valid()) {
    dictionary_.Bind(std::move(dictionary));
    dictionary_initialized_ = true;
    return;
  }

  MaybeRetryInitialize();
}

void SpellCheckLanguage::MaybeRetryInitialize() {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&RemoveDictionaryFile, dictionary_file_path_),
      base::BindOnce(&SpellCheckLanguage::OnFileRemovedForRetry,
                     weak_factory_.GetWeakPtr()));
}

void SpellCheckLanguage::OnFileRemovedForRetry(bool file_removed) {
  if (num_retries_ >= kMaxRetries) {
    LOG(ERROR) << "Service initialize failed after max retries.";
    service_.reset();
    return;
  }

  if (!file_removed) {
    LOG(ERROR) << "Will not retry - could not remove the dictionary file.";
    service_.reset();
    return;
  }

  ++num_retries_;
  Initialize(language_);
}

void SpellCheckLanguage::OnPathExistsComplete(bool path_exists) {
  // If the dictionary is not available, try to download it from the server.
  if (!path_exists) {
    auto url =
        GetDictionaryURL(dictionary_file_path_.BaseName().MaybeAsASCII());

    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = url;
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                               kNetworkTrafficAnnotationTag);

    loader_->SetRetryOptions(
        /*max_retries=*/3,
        network::SimpleURLLoader::RetryMode::RETRY_ON_5XX |
            network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

    loader_->DownloadToFile(
        url_loader_factory_.get(),
        base::BindOnce(&SpellCheckLanguage::OnSimpleURLLoaderComplete,
                       base::Unretained(this)),
        dictionary_file_path_);
    return;
  }

  InitializeSpellCheckService();
}

void SpellCheckLanguage::OnSaveDictionaryDataComplete(bool dictionary_saved) {
  if (!dictionary_saved) {
    MaybeRetryInitialize();
    return;
  }

  InitializeSpellCheckService();
}

void SpellCheckLanguage::OnOpenDictionaryFileComplete(base::File file) {
  service_->CreateDictionary(
      file.Duplicate(), base::BindOnce(&SpellCheckLanguage::OnDictionaryCreated,
                                       base::Unretained(this)));

  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&CloseDictionaryFile, std::move(file)));
}

}  // namespace quick_answers
