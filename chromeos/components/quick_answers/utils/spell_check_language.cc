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

void RemoveDictionaryFile(const base::FilePath& file_path) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  base::DeleteFile(file_path);
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
  dictionary_file_path_ = GetDictionaryFilePath(language);

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::PathExists, dictionary_file_path_),
      base::BindOnce(&SpellCheckLanguage::OnPathExistsComplete,
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
      FROM_HERE,
      base::BindOnce(&base::ReplaceFile, tmp_path, dictionary_file_path_,
                     nullptr),
      base::BindOnce(&SpellCheckLanguage::OnSaveDictionaryDataComplete,
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
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RemoveDictionaryFile, dictionary_file_path_));

  if (num_retries_ >= kMaxRetries) {
    LOG(ERROR) << "Service initialize failed after max retries";
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
        /*max_retries=*/5,
        network::SimpleURLLoader::RetryMode::RETRY_ON_5XX |
            network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

    loader_->DownloadToTempFile(
        url_loader_factory_.get(),
        base::BindOnce(&SpellCheckLanguage::OnSimpleURLLoaderComplete,
                       base::Unretained(this)));
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
